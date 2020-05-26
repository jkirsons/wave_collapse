#ifndef WAVE_COLLAPSE_H
#define WAVE_COLLAPSE_H

//#include "core/reference.h"
#include "modules/gridmap/grid_map.h"
#include <map>
#include <vector>
#include <set>

#define orth_index(i) Basis(Quat(Vector3(0, 1, 0), -0.5 * i  * M_PI)).get_orthogonal_index()

class WaveCollapse : public Node3D {
    GDCLASS(WaveCollapse, Node3D);

    struct Tile {
        int tile;
        unsigned short rotation;
        
        bool operator<(const Tile &o) const{
            return tile < o.tile;
        }
    };
    
    const std::vector<Vector3> directions = {Vector3(0,-1,0), Vector3(1,0,0), Vector3(0,1,0), Vector3(-1,0,0)};

    // internal indexes used by GridMap cell orientation
    const std::vector<int> orthagonal_indices = {orth_index(0), orth_index(1), orth_index(2), orth_index(3)};

    NodePath template_gridmap_path;
    GridMap* template_gridmap;

    NodePath output_gridmap_path;
    GridMap* output_gridmap;

    std::map<Vector3, int> template_map_tile;
    std::map<Vector3, int> template_map_rotation;

    std::map<int, int> weights; // tile, weight
    std::map<Vector3, Tile> resolved_tiles;
    std::map<Vector3, std::vector<unsigned char>> unresolved_tiles; // position, bitmask

    std::map<int, std::map<Tile, std::set<Tile>>> valid_combinations;  // direction, tile, other_tiles
    std::map<int, std::map<Tile, std::vector<unsigned char>>> valid_combinations_mask;  // direction, tile, other_tiles_mask
    
    std::map<Tile, int> tile_mask_index; // Tile --> int for bitmap mask

    template<typename T> T* _path_to_object(const NodePath &path);
    unsigned short _rotate_tile(int init_orth_index, int steps);
    int _rotate_direction(int init_direction, int steps);

protected:
    static void _bind_methods();

public:
    void set_template_gridmap(const NodePath &template_gridmap);
    NodePath get_template_gridmap() const;

    void set_output_gridmap(const NodePath &output_gridmap);
    NodePath get_output_gridmap() const;

    void generate_combinations();

    void _on_Player_position_changed(Vector3 position, int collapse_radius);

    WaveCollapse();
};

#endif // WAVE_COLLAPSE_H