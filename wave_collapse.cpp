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

    // Generate bitmask indexes
    int index = 1;
    for(auto const& [dir, tiles] : valid_combinations) {
        for(auto const& [tile, other_tiles] : tiles) {
            tile_mask_index[tile] = index;
            index++;
        }
    } 
}

/**
 * call a function for all bits in a bitmask
 */
void WaveCollapse::_bitmask_iterator(const std::vector<unsigned char>& bitmask, const std::function<void(int)>& func) {
    int index;
    for(const auto& part : bitmask) {
        // No bits in this part
        for(int i = 0; i < 8; i++) {
            if(part >> i == 0)
                break;
            if(part >> i && 1)
                func(index * 8 + i);
        }
        index++;
    }
}
  
/* Recursively get nibble of a given number  
and map them in the array */
unsigned int WaveCollapse::_countSetBitsRec(unsigned int num) 
{ 
    int nibble = 0; 
    if (0 == num) 
        return num_to_bits[0]; 
  
    // Find last nibble 
    nibble = num & 0xf; 
  
    // Use pre-stored values to find count 
    // in last nibble plus recursively add 
    // remaining nibbles. 
    return num_to_bits[nibble] + _countSetBitsRec(num >> 4); 
} 

float WaveCollapse::_shannon_entropy(Vector3 position)
{
    /*
		"""Calculates the Shannon Entropy of the wavefunction at `co_ords`."""
		var sum_of_weights = 0
		var sum_of_weight_log_weights = 0
		for opt in self.coefficients[Vector2(co_ords[0], co_ords[1])]:
			var weight = self.weights[opt]
			sum_of_weights += weight
			sum_of_weight_log_weights += log_weights[opt] #weight * log(weight)
		return log(sum_of_weights) - (sum_of_weight_log_weights / sum_of_weights)
    */
    //float sum_of_weights = 0.0;
    //float sum_of_weight_log_weights = 0.0;
    if(unresolved_tiles.find(position) != unresolved_tiles.end()) {
        unsigned int count = 0;
        for(auto const& part : unresolved_tiles[position]) {
            count += _countSetBitsRec(part);
        }
        float weight;
        _bitmask_iterator(unresolved_tiles[position], [&](int index){ weight += weights[index]; });

    }
    return 0.0;
}

WaveCollapse::WaveCollapse() {
    template_gridmap = nullptr;
    output_gridmap = nullptr;
}