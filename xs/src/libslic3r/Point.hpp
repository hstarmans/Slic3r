#ifndef slic3r_Point_hpp_
#define slic3r_Point_hpp_

#include "libslic3r.h"
#include <vector>
#include <math.h>
#include <string>
#include <sstream>
#include <unordered_map>

namespace Slic3r {

class Line;
class Linef;
class MultiPoint;
class Point;
class Pointf;
class Pointf3;
typedef Point Vector;
typedef Pointf Vectorf;
typedef Pointf3 Vectorf3;
typedef std::vector<Point> Points;
typedef std::vector<Point*> PointPtrs;
typedef std::vector<const Point*> PointConstPtrs;
typedef std::vector<Pointf> Pointfs;
typedef std::vector<Pointf3> Pointf3s;

class Point
{
    public:
    coord_t x;
    coord_t y;
    Point(coord_t _x = 0, coord_t _y = 0): x(_x), y(_y) {};
    Point(int _x, int _y): x(_x), y(_y) {};
    Point(long long _x, long long _y): x(_x), y(_y) {};  // for Clipper
    Point(double x, double y);
    static Point new_scale(coordf_t x, coordf_t y) {
        return Point(scale_(x), scale_(y));
    };
    bool operator==(const Point& rhs) const { return this->x == rhs.x && this->y == rhs.y; }
    std::string wkt() const;
    std::string dump_perl() const;
    void scale(double factor);
    void translate(double x, double y);
    void translate(const Vector &vector);
    void rotate(double angle);
    void rotate(double angle, const Point &center);
    Point rotated(double angle) const { Point res(*this); res.rotate(angle); return res; }
    Point rotated(double angle, const Point &center) const { Point res(*this); res.rotate(angle, center); return res; }
    bool coincides_with(const Point &point) const { return this->x == point.x && this->y == point.y; }
    bool coincides_with_epsilon(const Point &point) const;
    int nearest_point_index(const Points &points) const;
    int nearest_point_index(const PointConstPtrs &points) const;
    int nearest_point_index(const PointPtrs &points) const;
    size_t nearest_waypoint_index(const Points &points, const Point &point) const;
    bool nearest_point(const Points &points, Point* point) const;
    bool nearest_waypoint(const Points &points, const Point &dest, Point* point) const;
    double distance_to(const Point &point) const { return sqrt(distance_to_sq(point)); }
    double distance_to_sq(const Point &point) const { double dx = double(point.x - this->x); double dy = double(point.y - this->y); return dx*dx + dy*dy; }
    double distance_to(const Line &line) const;
    double perp_distance_to(const Line &line) const;
    double ccw(const Point &p1, const Point &p2) const;
    double ccw(const Line &line) const;
    double ccw_angle(const Point &p1, const Point &p2) const;
    Point projection_onto(const MultiPoint &poly) const;
    Point projection_onto(const Line &line) const;
    Point negative() const;
    Vector vector_to(const Point &point) const;
};

inline Point operator+(const Point& point1, const Point& point2) { return Point(point1.x + point2.x, point1.y + point2.y); }
inline Point operator-(const Point& point1, const Point& point2) { return Point(point1.x - point2.x, point1.y - point2.y); }
inline Point operator*(double scalar, const Point& point2) { return Point(scalar * point2.x, scalar * point2.y); }

// To be used by std::unordered_map, std::unordered_multimap and friends.
struct PointHash {
    size_t operator()(const Point &pt) const {
        return std::hash<coord_t>()(pt.x) ^ std::hash<coord_t>()(pt.y);
    }
};

// A generic class to search for a closest Point in a given radius.
// It uses std::unordered_multimap to implement an efficient 2D spatial hashing.
// The PointAccessor has to return const Point*.
// If a nullptr is returned, it is ignored by the query.
template<typename ValueType, typename PointAccessor> class ClosestPointInRadiusLookup
{
public:
    ClosestPointInRadiusLookup(coord_t search_radius, PointAccessor point_accessor = PointAccessor()) : 
		m_search_radius(search_radius), m_point_accessor(point_accessor), m_grid_log2(0)
    {
        // Resolution of a grid, twice the search radius + some epsilon.
		coord_t gridres = 2 * m_search_radius + 4;
        m_grid_resolution = gridres;
        assert(m_grid_resolution > 0);
        assert(m_grid_resolution < (coord_t(1) << 30));
		// Compute m_grid_log2 = log2(m_grid_resolution)
		if (m_grid_resolution > 32767) {
			m_grid_resolution >>= 16;
			m_grid_log2 += 16;
		}
		if (m_grid_resolution > 127) {
			m_grid_resolution >>= 8;
			m_grid_log2 += 8;
		}
		if (m_grid_resolution > 7) {
			m_grid_resolution >>= 4;
			m_grid_log2 += 4;
		}
		if (m_grid_resolution > 1) {
			m_grid_resolution >>= 2;
			m_grid_log2 += 2;
		}
		if (m_grid_resolution > 0)
			++ m_grid_log2;
		m_grid_resolution = 1 << m_grid_log2;
		assert(m_grid_resolution >= gridres);
		assert(gridres > m_grid_resolution / 2);
    }

    void insert(const ValueType &value) {
        const Point *pt = m_point_accessor(value);
        if (pt != nullptr)
            m_map.emplace(std::make_pair(Point(pt->x>>m_grid_log2, pt->y>>m_grid_log2), value));
    }

    void insert(ValueType &&value) {
        const Point *pt = m_point_accessor(value);
        if (pt != nullptr)
            m_map.emplace(std::make_pair(Point(pt->x>>m_grid_log2, pt->y>>m_grid_log2), std::move(value)));
    }

    // Return a pair of <ValueType*, distance_squared>
    std::pair<const ValueType*, double> find(const Point &pt) {
        // Iterate over 4 closest grid cells around pt,
        // find the closest start point inside these cells to pt.
        const ValueType *value_min = nullptr;
        double           dist_min = std::numeric_limits<double>::max();
        // Round pt to a closest grid_cell corner.
        Point            grid_corner((pt.x+(m_grid_resolution>>1))>>m_grid_log2, (pt.y+(m_grid_resolution>>1))>>m_grid_log2);
        // For four neighbors of grid_corner:
        for (coord_t neighbor_y = -1; neighbor_y < 1; ++ neighbor_y) {
            for (coord_t neighbor_x = -1; neighbor_x < 1; ++ neighbor_x) {
                // Range of fragment starts around grid_corner, close to pt.
                auto range = m_map.equal_range(Point(grid_corner.x + neighbor_x, grid_corner.y + neighbor_y));
                // Find the map entry closest to pt.
                for (auto it = range.first; it != range.second; ++it) {
                    const ValueType &value = it->second;
                    const Point *pt2 = m_point_accessor(value);
                    if (pt2 != nullptr) {
                        const double d2 = pt.distance_to_sq(*pt2);
                        if (d2 < dist_min) {
                            dist_min = d2;
                            value_min = &value;
                        }
                    }
                }
            }
        }
        return (value_min != nullptr && dist_min < coordf_t(m_search_radius * m_search_radius)) ? 
            std::make_pair(value_min, dist_min) : 
            std::make_pair(nullptr, std::numeric_limits<double>::max());
    }

private:
    typedef typename std::unordered_multimap<Point, ValueType, PointHash> map_type;
    PointAccessor m_point_accessor;
    map_type m_map;
    coord_t  m_search_radius;
    coord_t  m_grid_resolution;
    coord_t  m_grid_log2;
};

class Point3 : public Point
{
    public:
    coord_t z;
    explicit Point3(coord_t _x = 0, coord_t _y = 0, coord_t _z = 0): Point(_x, _y), z(_z) {};
};

std::ostream& operator<<(std::ostream &stm, const Pointf &pointf);

class Pointf
{
    public:
    coordf_t x;
    coordf_t y;
    explicit Pointf(coordf_t _x = 0, coordf_t _y = 0): x(_x), y(_y) {};
    static Pointf new_unscale(coord_t x, coord_t y) {
        return Pointf(unscale(x), unscale(y));
    };
    static Pointf new_unscale(const Point &p) {
        return Pointf(unscale(p.x), unscale(p.y));
    };
    std::string wkt() const;
    std::string dump_perl() const;
    void scale(double factor);
    void translate(double x, double y);
    void translate(const Vectorf &vector);
    void rotate(double angle);
    void rotate(double angle, const Pointf &center);
    Pointf negative() const;
    Vectorf vector_to(const Pointf &point) const;
};

inline Pointf operator+(const Pointf& point1, const Pointf& point2) { return Pointf(point1.x + point2.x, point1.y + point2.y); }
inline Pointf operator-(const Pointf& point1, const Pointf& point2) { return Pointf(point1.x - point2.x, point1.y - point2.y); }
inline Pointf operator*(double scalar, const Pointf& point2) { return Pointf(scalar * point2.x, scalar * point2.y); }
inline Pointf operator*(const Pointf& point2, double scalar) { return Pointf(scalar * point2.x, scalar * point2.y); }
inline coordf_t cross(const Pointf &v1, const Pointf &v2) { return v1.x * v2.y - v1.y * v2.x; }
inline coordf_t dot(const Pointf &v1, const Pointf &v2) { return v1.x * v2.x + v1.y * v2.y; }
inline coordf_t dot(const Pointf &v) { return v.x * v.x + v.y * v.y; }
inline double length(const Vectorf &v) { return sqrt(dot(v)); }
inline double l2(const Vectorf &v) { return dot(v); }

class Pointf3 : public Pointf
{
    public:
    coordf_t z;
    explicit Pointf3(coordf_t _x = 0, coordf_t _y = 0, coordf_t _z = 0): Pointf(_x, _y), z(_z) {};
    static Pointf3 new_unscale(coord_t x, coord_t y, coord_t z) {
        return Pointf3(unscale(x), unscale(y), unscale(z));
    };
    void scale(double factor);
    void translate(const Vectorf3 &vector);
    void translate(double x, double y, double z);
    double distance_to(const Pointf3 &point) const;
    Pointf3 negative() const;
    Vectorf3 vector_to(const Pointf3 &point) const;
};

} // namespace Slic3r

// start Boost
#include <boost/version.hpp>
#include <boost/polygon/polygon.hpp>
namespace boost { namespace polygon {
    template <>
    struct geometry_concept<coord_t> { typedef coordinate_concept type; };
    
/* Boost.Polygon already defines a specialization for coordinate_traits<long> as of 1.60:
   https://github.com/boostorg/polygon/commit/0ac7230dd1f8f34cb12b86c8bb121ae86d3d9b97 */
#if BOOST_VERSION < 106000
    template <>
    struct coordinate_traits<coord_t> {
        typedef coord_t coordinate_type;
        typedef long double area_type;
        typedef long long manhattan_area_type;
        typedef unsigned long long unsigned_area_type;
        typedef long long coordinate_difference;
        typedef long double coordinate_distance;
    };
#endif

    template <>
    struct geometry_concept<Slic3r::Point> { typedef point_concept type; };
   
    template <>
    struct point_traits<Slic3r::Point> {
        typedef coord_t coordinate_type;
    
        static inline coordinate_type get(const Slic3r::Point& point, orientation_2d orient) {
            return (orient == HORIZONTAL) ? point.x : point.y;
        }
    };
    
    template <>
    struct point_mutable_traits<Slic3r::Point> {
        typedef coord_t coordinate_type;
        static inline void set(Slic3r::Point& point, orientation_2d orient, coord_t value) {
            if (orient == HORIZONTAL)
                point.x = value;
            else
                point.y = value;
        }
        static inline Slic3r::Point construct(coord_t x_value, coord_t y_value) {
            Slic3r::Point retval;
            retval.x = x_value;
            retval.y = y_value; 
            return retval;
        }
    };
} }
// end Boost

#endif
