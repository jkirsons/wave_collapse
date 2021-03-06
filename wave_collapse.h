#ifndef WAVE_COLLAPSE_H
#define WAVE_COLLAPSE_H

//#include "core/reference.h"
#include "modules/gridmap/grid_map.h"
#include <map>
#include <vector>
#include <set>
#include <functional>
#include <thread>
#include <chrono>
#include <future>
#include <iostream>
#include "robin_hood.h"

#define orth_index(i) Basis(Quat(Vector3(0, 1, 0), -0.5 * i * 3.14159265358979323846)).get_orthogonal_index()


template <> struct std::hash<Vector3i>
{
    std::size_t operator()(const Vector3i& k) const
    {
       // Compute individual hash values for first,
        // second and third and combine them using XOR
        // and bit shifting:
        return ((std::hash<float>()(k.x)
                ^ (std::hash<float>()(k.y) << 1)) >> 1)
                ^ (std::hash<float>()(k.z) << 1);
    }
};

struct Tile {
    int tile;
    unsigned short rotation;
    Tile() {}
    Tile(int t, unsigned short r) : tile(t), rotation(r) {} 
    
    bool const operator<(const Tile &o) const {
        return std::tie(tile, rotation) < std::tie(o.tile, o.rotation);
    }

    bool const operator==(const Tile &o) const {
        return tile == o.tile && rotation == o.rotation;
    }
};

template <> struct std::hash<Tile>
{
    std::size_t operator()(const Tile& k) const
    {
        return ((k.tile << 16) + k.rotation);
    }  
};

class WaveCollapse : public Node3D {
    GDCLASS(WaveCollapse, Node3D);

#define segment_type unsigned long long
#define map_type robin_hood::unordered_map
//#define map_type std::unordered_map

    const int bits_per_segment = std::numeric_limits<segment_type>::digits;
    
    const std::vector<Vector3i> directions = {Vector3i(0,0,-1), Vector3i(1,0,0), Vector3i(0,0,1), Vector3i(-1,0,0)};

    // internal indexes used by GridMap cell orientation
    const std::vector<int> orthagonal_indices = {orth_index(0), orth_index(1), orth_index(2), orth_index(3)};

    NodePath template_gridmap_path;
    GridMap* template_gridmap;

    NodePath output_gridmap_path;
    GridMap* output_gridmap;

    std::map<Vector3i, int> template_map_tile;
    std::map<Vector3i, int> template_map_rotation;

    map_type<int, int> weights; // tile, weight
    map_type<Vector3i, Tile> resolved_tiles;
    std::unordered_map<Vector3i, std::vector<segment_type>> unresolved_tiles; // position, bitmask

    map_type<int, map_type<Tile, std::set<Tile>>> valid_combinations;  // direction, tile, other_tiles
    map_type<int, map_type<Tile, std::vector<segment_type>>> valid_combinations_mask;  // direction, tile, other_tiles_mask
    
    map_type<Tile, int> tile_mask_index; // Tile --> int for bitmap mask
    map_type<int, Tile> tile_mask_reverse_index;

    std::mutex g_mutex;
    std::future<void> g_future;
    bool terminate_thread = false;

    template<typename T> T* _path_to_object(const NodePath &path);
    unsigned short _rotate_tile(int init_orth_index, int steps);
    int _rotate_direction(int init_direction, int steps);
    float _shannon_entropy(Vector3i position);

    void for_each_tile_in_bitmask(const std::vector<segment_type> &bitmask, const std::function<void(int)> &func);
    void _bitmask_set_valid(const int& dir, const int& tile, const int& other_tile);
    Vector3i _min_entropy_co_ords();
    void _propagate(const Vector3i& position);
    void _iterate();
    void _collapse(const Vector3i& position);
    void _new_tile(const Vector3i& position);
    void _setup(int y);
    
    bool _within_radius(const Vector3i& position);

    Vector3i player_position;
    int radius = 0;
    int radius_squared = 0;
    bool setup_done = false;
	std::vector<segment_type> all_combinations_bitmask;

protected:
    static void _bind_methods();

public:
    void set_template_gridmap(const NodePath &template_gridmap);
    NodePath get_template_gridmap() const;

    void set_output_gridmap(const NodePath &output_gridmap);
    NodePath get_output_gridmap() const;

    void generate_combinations();

    void _on_Player_position_changed(const Vector3i& position, const int& collapse_radius);
    void process();
    void process_thread();

    WaveCollapse();
    ~WaveCollapse();
};

#endif // WAVE_COLLAPSE_H
