#include "wave_collapse.h"
#include <algorithm>

void WaveCollapse::_on_Player_position_changed(Vector3 position, int collapse_radius)
{

}

template<typename T>
T* WaveCollapse::_path_to_object(const NodePath &path)
{
    T* obj;
    if (!has_node(path)) {
        return nullptr;
    }
    Node *n = get_node(path);
    if (!n) {
        return nullptr;
    }
    obj = Object::cast_to<T>(n);
    if (!obj) {
        return nullptr;
    }
    return obj;
}

void WaveCollapse::set_template_gridmap(const NodePath &template_gridmap) {
    this->template_gridmap_path = template_gridmap;
    this->template_gridmap = _path_to_object<GridMap>(template_gridmap);
    generate_combinations();
}

NodePath WaveCollapse::get_template_gridmap() const {
    return template_gridmap_path;
}

void WaveCollapse::set_output_gridmap(const NodePath &output_gridmap) {
    this->output_gridmap_path = output_gridmap;
    this->output_gridmap = _path_to_object<GridMap>(output_gridmap);
}

NodePath WaveCollapse::get_output_gridmap() const {
    return output_gridmap_path;
}

void WaveCollapse::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_template_gridmap"), &WaveCollapse::set_template_gridmap);
    ClassDB::bind_method(D_METHOD("get_template_gridmap"), &WaveCollapse::get_template_gridmap);

    ClassDB::bind_method(D_METHOD("set_output_gridmap"), &WaveCollapse::set_output_gridmap);
    ClassDB::bind_method(D_METHOD("get_output_gridmap"), &WaveCollapse::get_output_gridmap);

    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "Template Gridmap", PROPERTY_HINT_NODE_PATH_VALID_TYPES), "set_template_gridmap", "get_template_gridmap");
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "Output Gridmap", PROPERTY_HINT_NODE_PATH_VALID_TYPES), "set_output_gridmap", "get_output_gridmap");
    ADD_SIGNAL(MethodInfo("cell_changed", PropertyInfo(Variant::VECTOR3, "cell"), PropertyInfo(Variant::INT, "tile"), PropertyInfo(Variant::INT, "orientation")));

}

unsigned short WaveCollapse::_rotate_tile(int init_orth_index, int steps) {
    unsigned short dir = 0;
    for(unsigned int i = 0; i < orthagonal_indices.size(); i++) {
        if(orthagonal_indices[i] == init_orth_index) {
            dir = i;
            break;
        }
    }
    dir += steps;
    if((std::size_t)dir >= orthagonal_indices.size())
        dir -= orthagonal_indices.size(); 
    return dir;
}

int WaveCollapse::_rotate_direction(int init_direction, int steps) {
    int dir = init_direction + steps;
    if((std::size_t)dir >= directions.size())
        dir -= directions.size(); 
    return dir;
}

void WaveCollapse::generate_combinations() {
    if(template_gridmap == nullptr)
        return;

    std::vector<int> symetric_tiles = {9, 15, 16};
    std::sort(symetric_tiles.begin(), symetric_tiles.end());

    // Create maps of tiles and rotations
    Array cells = template_gridmap->get_used_cells();
    for (int i = 0; i < cells.size(); i++) {
        const Vector3& vec = (Vector3)cells[i];
        template_map_tile.emplace(std::pair<Vector3, int>(vec, 
            template_gridmap->get_cell_item(vec.x, vec.y, vec.z)));

        template_map_rotation.emplace(std::pair<Vector3, int>(vec,
            template_gridmap->get_cell_item_orientation(vec.x, vec.y, vec.z)));
    }

    // Generate combinations
    for(auto const& [pos, tile] : template_map_tile) {
        for(unsigned int d = 0; d < directions.size(); d++) {
            Vector3 other_pos = pos + directions[d];
            if(template_map_tile.count(other_pos) > 0) {
                for(int r = 0; r < 4; r++) {
                    unsigned short tile_rot = _rotate_tile(template_map_rotation[pos], r);
                    int other_tile = template_map_rotation[other_pos];
                    unsigned short other_tile_rot = _rotate_tile(template_map_rotation[other_pos], r);
                    int direction_index = _rotate_direction(d, r);

                    if(std::binary_search(symetric_tiles.begin(), symetric_tiles.end(), tile)) {
                        tile_rot = 0;
                    }
                    if(std::binary_search(symetric_tiles.begin(), symetric_tiles.end(), other_tile)) {
                        other_tile_rot = 0;
                    }

                    if(d == 0)
                        weights[tile] += 1;
                    
                    valid_combinations[direction_index][Tile{tile, tile_rot}].insert(Tile{other_tile, other_tile_rot});
                }
            }
        }
    }

    // Generate bitmasks
    for(auto const& [dir, tiles] : valid_combinations) {
        for(auto const& [tile, other_tiles] : tiles) {
            
        }
    } 
}

WaveCollapse::WaveCollapse() {
    template_gridmap = nullptr;
    output_gridmap = nullptr;
}