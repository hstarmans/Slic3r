#ifndef slic3r_3DScene_hpp_
#define slic3r_3DScene_hpp_

#include "../../libslic3r/libslic3r.h"
#include "../../libslic3r/Point.hpp"
#include "../../libslic3r/Line.hpp"
#include "../../libslic3r/TriangleMesh.hpp"

namespace Slic3r {

class Print;
class PrintObject;
class Model;
class ModelObject;

// A container for interleaved arrays of 3D vertices and normals,
// possibly indexed by triangles and / or quads.
class GLIndexedVertexArray {
public:
    GLIndexedVertexArray() : 
        vertices_and_normals_interleaved_VBO_id(0),
        triangle_indices_VBO_id(0),
        quad_indices_VBO_id(0)
        { this->setup_sizes(); }
    GLIndexedVertexArray(const GLIndexedVertexArray &rhs) :
        vertices_and_normals_interleaved(rhs.vertices_and_normals_interleaved),
        triangle_indices(rhs.triangle_indices),
        quad_indices(rhs.quad_indices),
        vertices_and_normals_interleaved_VBO_id(0),
        triangle_indices_VBO_id(0),
        quad_indices_VBO_id(0)
        { this->setup_sizes(); }
    GLIndexedVertexArray(GLIndexedVertexArray &&rhs) :
        vertices_and_normals_interleaved(std::move(rhs.vertices_and_normals_interleaved)),
        triangle_indices(std::move(rhs.triangle_indices)),
        quad_indices(std::move(rhs.quad_indices)),
        vertices_and_normals_interleaved_VBO_id(0),
        triangle_indices_VBO_id(0),
        quad_indices_VBO_id(0)
        { this->setup_sizes(); }

    GLIndexedVertexArray& operator=(const GLIndexedVertexArray &rhs)
    {
        assert(vertices_and_normals_interleaved_VBO_id == 0);
        assert(triangle_indices_VBO_id == 0);
        assert(triangle_indices_VBO_id == 0);
        this->vertices_and_normals_interleaved = rhs.vertices_and_normals_interleaved;
        this->triangle_indices                 = rhs.triangle_indices;
        this->quad_indices                     = rhs.quad_indices;
        this->setup_sizes();
        return *this;
    }

    GLIndexedVertexArray& operator=(GLIndexedVertexArray &&rhs) 
    {
        assert(vertices_and_normals_interleaved_VBO_id == 0);
        assert(triangle_indices_VBO_id == 0);
        assert(triangle_indices_VBO_id == 0);
        this->vertices_and_normals_interleaved = std::move(rhs.vertices_and_normals_interleaved);
        this->triangle_indices                 = std::move(rhs.triangle_indices);
        this->quad_indices                     = std::move(rhs.quad_indices);
        this->setup_sizes();
        return *this;
    }

    // Vertices and their normals, interleaved to be used by void glInterleavedArrays(GL_N3F_V3F, 0, x)
    std::vector<float> vertices_and_normals_interleaved;
    std::vector<int>   triangle_indices;
    std::vector<int>   quad_indices;

    // When the geometry data is loaded into the graphics card as Vertex Buffer Objects,
    // the above mentioned std::vectors are cleared and the following variables keep their original length.
    size_t             vertices_and_normals_interleaved_size;
    size_t             triangle_indices_size;
    size_t             quad_indices_size;

    // IDs of the Vertex Array Objects, into which the geometry has been loaded.
    // Zero if the VBOs are not used.
    unsigned int       vertices_and_normals_interleaved_VBO_id;
    unsigned int       triangle_indices_VBO_id;
    unsigned int       quad_indices_VBO_id;

    void load_mesh_flat_shading(const TriangleMesh &mesh);

    inline bool has_VBOs() const { return vertices_and_normals_interleaved_VBO_id != 0; }

    inline void reserve(size_t sz) {
        this->vertices_and_normals_interleaved.reserve(sz * 6);
        this->triangle_indices.reserve(sz * 3);
        this->quad_indices.reserve(sz * 4);
    }

    inline void push_geometry(float x, float y, float z, float nx, float ny, float nz) {
        this->vertices_and_normals_interleaved.reserve(this->vertices_and_normals_interleaved.size() + 6);
        this->vertices_and_normals_interleaved.push_back(nx);
        this->vertices_and_normals_interleaved.push_back(ny);
        this->vertices_and_normals_interleaved.push_back(nz);
        this->vertices_and_normals_interleaved.push_back(x);
        this->vertices_and_normals_interleaved.push_back(y);
        this->vertices_and_normals_interleaved.push_back(z);
    };

    inline void push_geometry(double x, double y, double z, double nx, double ny, double nz) {
        push_geometry(float(x), float(y), float(z), float(nx), float(ny), float(nz));
    }

    // Finalize the initialization of the geometry & indices,
    // upload the geometry and indices to OpenGL VBO objects
    // and shrink the allocated data, possibly relasing it if it has been loaded into the VBOs.
    void finalize_geometry(bool use_VBOs);
    // Release the geometry data, release OpenGL VBOs.
    void release_geometry();
    // Render either using an immediate mode, or the VBOs.
    void render() const;
    void render(const std::pair<size_t, size_t> &tverts_range, const std::pair<size_t, size_t> &qverts_range) const;

    // Is there any geometry data stored?
    bool empty() const { return vertices_and_normals_interleaved_size == 0; }

    // Is this object indexed, or is it just a set of triangles?
    bool indexed() const { return ! this->empty() && this->triangle_indices_size + this->quad_indices_size > 0; }

    void clear() {
        this->vertices_and_normals_interleaved.clear();
        this->triangle_indices.clear();
        this->quad_indices.clear();
        this->setup_sizes();
    }

    // Shrink the internal storage to tighly fit the data stored.
    void shrink_to_fit() { 
        if (! this->has_VBOs())
            this->setup_sizes();
        this->vertices_and_normals_interleaved.shrink_to_fit();
        this->triangle_indices.shrink_to_fit();
        this->quad_indices.shrink_to_fit();
    }

    BoundingBoxf3 bounding_box() const {
        BoundingBoxf3 bbox;
        if (! this->vertices_and_normals_interleaved.empty()) {
            bbox.min.x = bbox.max.x = this->vertices_and_normals_interleaved[3];
            bbox.min.y = bbox.max.y = this->vertices_and_normals_interleaved[4];
            bbox.min.z = bbox.max.z = this->vertices_and_normals_interleaved[5];
            for (size_t i = 9; i < this->vertices_and_normals_interleaved.size(); i += 6) {
                const float *verts = this->vertices_and_normals_interleaved.data() + i;
                bbox.min.x = std::min<coordf_t>(bbox.min.x, verts[0]);
                bbox.min.y = std::min<coordf_t>(bbox.min.y, verts[1]);
                bbox.min.z = std::min<coordf_t>(bbox.min.z, verts[2]);
                bbox.max.x = std::max<coordf_t>(bbox.max.x, verts[0]);
                bbox.max.y = std::max<coordf_t>(bbox.max.y, verts[1]);
                bbox.max.z = std::max<coordf_t>(bbox.max.z, verts[2]);
            }
        }
        return bbox;
    }

private:
    inline void setup_sizes() {
        vertices_and_normals_interleaved_size = this->vertices_and_normals_interleaved.size();
        triangle_indices_size                 = this->triangle_indices.size();
        quad_indices_size                     = this->quad_indices.size();
    }
};

class GLTexture
{
public:
    GLTexture() : width(0), height(0), levels(0), cells(0) {}

    // Texture data
    std::vector<char>   data;
    // Width of the texture, top level.
    size_t              width;
    // Height of the texture, top level.
    size_t              height;
    // For how many levels of detail is the data allocated?
    size_t              levels;
    // Number of texture cells allocated for the height texture.
    size_t              cells;
};

class GLVolume {
public:
    GLVolume(float r = 1.f, float g = 1.f, float b = 1.f, float a = 1.f) :
        composite_id(-1),
        select_group_id(-1),
        drag_group_id(-1),
        selected(false),
        hover(false),
        qverts_range(0, size_t(-1)),
        tverts_range(0, size_t(-1))
    {
        color[0] = r;
        color[1] = g;
        color[2] = b;
        color[3] = a;
    }
    GLVolume(const float *rgba) : GLVolume(rgba[0], rgba[1], rgba[2], rgba[3]) {}

    std::vector<int> load_object(
        const ModelObject        *model_object, 
        const std::vector<int>   &instance_idxs,
        const std::string        &color_by,
        const std::string        &select_by,
        const std::string        &drag_by);

    // Bounding box of this volume, in unscaled coordinates.
    BoundingBoxf3       bounding_box;
    // Offset of the volume to be rendered.
    Pointf3             origin;
    // Color of the triangles / quads held by this volume.
    float               color[4];

    // An ID containing the object ID, volume ID and instance ID.
    int                 composite_id;
    // An ID for group selection. It may be the same for all meshes of all object instances, or for just a single object instance.
    int                 select_group_id;
    // An ID for group dragging. It may be the same for all meshes of all object instances, or for just a single object instance.
    int                 drag_group_id;
    // Is this object selected?
    bool                selected;
    // Boolean: Is mouse over this object?
    bool                hover;

    // Interleaved triangles & normals with indexed triangles & quads.
    GLIndexedVertexArray        indexed_vertex_array;
    // Ranges of triangle and quad indices to be rendered.
    std::pair<size_t, size_t>   tverts_range;
    std::pair<size_t, size_t>   qverts_range;

    // If the qverts or tverts contain thick extrusions, then offsets keeps pointers of the starts
    // of the extrusions per layer.
    std::vector<coordf_t>       print_zs;
    // Offset into qverts & tverts, or offsets into indices stored into an OpenGL name_index_buffer.
    std::vector<size_t>         offsets;

    int                 object_idx() const { return this->composite_id / 1000000; }
    int                 volume_idx() const { return (this->composite_id / 1000) % 1000; }
    int                 instance_idx() const { return this->composite_id % 1000; }
    BoundingBoxf3       transformed_bounding_box() const { BoundingBoxf3 bb = this->bounding_box; bb.translate(this->origin); return bb; }

    bool                empty() const { return this->indexed_vertex_array.empty(); }
    bool                indexed() const { return this->indexed_vertex_array.indexed(); }

    void                set_range(coordf_t low, coordf_t high);
    void                render() const;
    void                release_geometry() { this->indexed_vertex_array.release_geometry(); }

    /************************************************ Layer height texture ****************************************************/
    std::shared_ptr<GLTexture>  layer_height_texture;

    bool                has_layer_height_texture() const 
        { return this->layer_height_texture.get() != nullptr; }
    size_t              layer_height_texture_width() const 
        { return (this->layer_height_texture.get() == nullptr) ? 0 : this->layer_height_texture->width; }
    size_t              layer_height_texture_height() const 
        { return (this->layer_height_texture.get() == nullptr) ? 0 : this->layer_height_texture->height; }
    size_t              layer_height_texture_cells() const 
        { return (this->layer_height_texture.get() == nullptr) ? 0 : this->layer_height_texture->cells; }
    void*               layer_height_texture_data_ptr_level0() {
        return (layer_height_texture.get() == nullptr) ? 0 :
            (void*)layer_height_texture->data.data();
    }
    void*               layer_height_texture_data_ptr_level1() {
        return (layer_height_texture.get() == nullptr) ? 0 :
            (void*)(layer_height_texture->data.data() + layer_height_texture->width * layer_height_texture->height * 4);
    }
    double              layer_height_texture_z_to_row_id() const { 
        return (this->layer_height_texture.get() == nullptr) ? 0. : 
            double(this->layer_height_texture->cells - 1) / (double(this->layer_height_texture->width) * bounding_box.max.z);
    }
    void                generate_layer_height_texture(PrintObject *print_object, bool force);
};

class GLVolumeCollection
{
public:
    std::vector<GLVolume*> volumes;
    
    GLVolumeCollection() {};
    ~GLVolumeCollection() { clear(); };

    std::vector<int> load_object(
        const ModelObject       *model_object,
        int                      obj_idx,
        const std::vector<int>  &instance_idxs,
        const std::string       &color_by,
        const std::string       &select_by,
        const std::string       &drag_by,
        bool                     use_VBOs);

    // Render the volumes by OpenGL.
    void render_VBOs() const;
    void render_legacy() const;
    // Release the geometry data assigned to the volumes.
    // If OpenGL VBOs were allocated, an OpenGL context has to be active to release them.
    void release_geometry() { for (auto *v : volumes) v->release_geometry(); }
    // Clear the geometry
    void clear() { for (auto *v : volumes) delete v; volumes.clear(); }

    bool empty() const { return volumes.empty(); }
    void set_range(double low, double high) { for (GLVolume *vol : this->volumes) vol->set_range(low, high); }

private:
    GLVolumeCollection(const GLVolumeCollection &other);
    GLVolumeCollection& operator=(const GLVolumeCollection &);
};

class _3DScene
{
public:
    static void _glew_init();

    static void _load_print_toolpaths(
        const Print         *print,
        GLVolumeCollection  *volumes,
        bool                 use_VBOs);

    static void _load_print_object_toolpaths(
        const PrintObject   *print_object,
        GLVolumeCollection  *volumes,
        bool                 use_VBOs);
};

}

#endif
