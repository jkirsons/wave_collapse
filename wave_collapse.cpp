#include "wave_collapse.h"
#include <algorithm>

WaveCollapse::WaveCollapse() {
    setup_done = false;
}

WaveCollapse::~WaveCollapse() {
    if(g_future.valid()) {
        terminate_thread = true;
        g_future.wait();
    }
}

void WaveCollapse::_bind_methods() {
    // exposed methods
    ClassDB::bind_method(D_METHOD("_on_Player_position_changed"), &WaveCollapse::_on_Player_position_changed);
    ClassDB::bind_method(D_METHOD("process_thread"), &WaveCollapse::process_thread);
    ClassDB::bind_method(D_METHOD("set_template_gridmap"), &WaveCollapse::set_template_gridmap);
    ClassDB::bind_method(D_METHOD("get_template_gridmap"), &WaveCollapse::get_template_gridmap);
    ClassDB::bind_method(D_METHOD("set_output_gridmap"), &WaveCollapse::set_output_gridmap);
    ClassDB::bind_method(D_METHOD("get_output_gridmap"), &WaveCollapse::get_output_gridmap);

    // inspector properties
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "Template Gridmap", PROPERTY_HINT_NODE_PATH_VALID_TYPES), "set_template_gridmap", "get_template_gridmap");
    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "Output Gridmap", PROPERTY_HINT_NODE_PATH_VALID_TYPES), "set_output_gridmap", "get_output_gridmap");
    
    // signals emitted
    ADD_SIGNAL(MethodInfo("cell_changed", PropertyInfo(Variant::VECTOR3I, "cell"), PropertyInfo(Variant::INT, "tile"), PropertyInfo(Variant::INT, "orientation")));
}

void WaveCollapse::set_template_gridmap(const NodePath &template_gridmap) {
    this->template_gridmap_path = template_gridmap;
    this->template_gridmap = _path_to_object<GridMap>(template_gridmap);
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
    if((std::size_t)dir >= directions.size()) {
        dir -= directions.size();
    }
    return dir;
}

/**
 * call a function for all bits in a bitmask
 */
void WaveCollapse::for_each_tile_in_bitmask(const std::vector<segment_type>& bitmask, const std::function<void(int)>& func) {
    int index = 0;
    for(const auto& part : bitmask) {
        for(int i = 0; i < bits_per_segment; i++) {
            // No bits in this part
            if(part >> i == 0)
                break;
            if((part >> i) & 1)
                func(index * bits_per_segment + i);
        }
        index += 1;
    }
}

/**
 * sets the flag for one tile in the valid bitmask
 */
void WaveCollapse::_bitmask_set_valid(const int& dir, const int& tile, const int& other_tile) {
	segment_type other_tile_int = pow(2, other_tile);

    // No valid mask yet
    if(valid_combinations_mask[dir].find(tile_mask_reverse_index[tile]) == valid_combinations_mask[dir].end()) {
        for(int i = 0;  i < ceil(tile_mask_index.size() / (float)bits_per_segment); i++) {
            valid_combinations_mask[dir][tile_mask_reverse_index[tile]].push_back(0x00);
        }
    }

    // Set bit
    for(auto& part : valid_combinations_mask[dir][tile_mask_reverse_index[tile]]) {
        if(other_tile_int < std::pow(2, bits_per_segment)) {
            part = part | other_tile_int;
        }
        other_tile_int >>= bits_per_segment;
        if(other_tile_int == 0) {
            break;
        }
    }
}

void WaveCollapse::generate_combinations() {
    if(template_gridmap == nullptr)
        return;

    std::vector<int> symetric_tiles = {11, 17, 18};
    std::sort(symetric_tiles.begin(), symetric_tiles.end());

    // Create maps of tiles and rotations
    Array cells = template_gridmap->get_used_cells();
    for (int i = 0; i < cells.size(); i++) {
        const Vector3i& vec = (Vector3i)cells[i];
        template_map_tile.emplace(std::pair<Vector3i, int>(vec, 
            template_gridmap->get_cell_item(vec)));

        template_map_rotation.emplace(std::pair<Vector3i, int>(vec,
            template_gridmap->get_cell_item_orientation(vec)));
    }

    // Generate valid tile combinations
    int index = 1;    
    for(auto const& [pos, tile] : template_map_tile) {
        for(unsigned int d = 0; d < directions.size(); ++d) {
            Vector3i other_pos = pos + directions[d];
            if(template_map_tile.count(other_pos) > 0) {
                for(int r = 0; r < 4; r++) {
                    unsigned short tile_rot = _rotate_tile(template_map_rotation[pos], r);
                    int other_tile = template_map_tile[other_pos];
                    unsigned short other_tile_rot = _rotate_tile(template_map_rotation[other_pos], r);
                    int direction_index = _rotate_direction(d, r);

                    if(std::binary_search(symetric_tiles.begin(), symetric_tiles.end(), tile)) {
                        tile_rot = 0;
                    }
                    if(std::binary_search(symetric_tiles.begin(), symetric_tiles.end(), other_tile)) {
                        other_tile_rot = 0;
                    }
                    
                    Tile t = Tile(tile, orthagonal_indices[tile_rot]);
                    Tile ot = Tile(other_tile, orthagonal_indices[other_tile_rot]);

                    valid_combinations[direction_index][t].insert(ot);

                    // Generate bitmask indexes
                    if(tile_mask_index.find(t) == tile_mask_index.end()) {
                        tile_mask_index[t] = index;
                        tile_mask_reverse_index[index] = t;
                        index++;
                    }

                    weights[tile_mask_index[t]] += 1;                   
                }
            }
        }
    }

	all_combinations_bitmask = {};
    for(unsigned int d = 0; d < directions.size(); ++d) {
        for(const auto& [tile, other_tiles] : valid_combinations[d]) {
            for(const auto& other_tile : other_tiles) {
				// set valid combinations bitmask
                _bitmask_set_valid(d, tile_mask_index[tile], tile_mask_index[other_tile]);

				// generate initial tile mask
				if (all_combinations_bitmask.empty()) {
					all_combinations_bitmask = valid_combinations_mask[d][tile];
				} else {
					for (int i = 0; i < (int)all_combinations_bitmask.size(); ++i) {
						all_combinations_bitmask[i] |= valid_combinations_mask[d][tile][i];
					}
				}
            }
        }
    }
	
}

float WaveCollapse::_shannon_entropy(Vector3i position)
{
    float sum_of_weights = 0.0;
    float sum_of_weight_log_weights = 0.0;
    if(unresolved_tiles.find(position) != unresolved_tiles.end()) {
        for_each_tile_in_bitmask(unresolved_tiles[position], [&](int index){ 
            sum_of_weights += weights[index]; 
            if(weights[index] != 0) {
                sum_of_weight_log_weights += (weights[index] * log(weights[index]));
            }
        });

    }
    if(sum_of_weights == 0) {
        return 999.0;
    }
    return log(sum_of_weights) - (sum_of_weight_log_weights / sum_of_weights);
}

Vector3i WaveCollapse::_min_entropy_co_ords() {
    float min_entropy = 0.0;
    Vector3i min_entropy_co_ords = Vector3i();
    for(const auto& tile : unresolved_tiles) {
        Vector3i diff = tile.first - player_position;
        if(diff.x*diff.x + diff.z*diff.z <= radius_squared) {
            float entropy = _shannon_entropy(tile.first) + (rand() % 100) / 100000.0;
            if( min_entropy == 0.0 || entropy < min_entropy) {
                min_entropy = entropy;
                min_entropy_co_ords = tile.first;
            }
        }
    }
    return min_entropy_co_ords;
}

void WaveCollapse::_new_tile(const Vector3i& position) {
	/*
	int size = tile_mask_index.size();
    for(int i = 0;  i < ceil( (float)size / (float)bits_per_segment); ++i) {
		segment_type val = std::min((segment_type)std::pow(2, bits_per_segment) - 1,
			(segment_type)std::pow(2, size - i * bits_per_segment + 1 ) - 1);
		unresolved_tiles[position].push_back(val);
    }
	*/
	unresolved_tiles[position] = all_combinations_bitmask;
}

void WaveCollapse::_collapse(const Vector3i& position) {
    unsigned long total_weights = 0;
    int chosen = -1;
    auto iterator = unresolved_tiles.find(position);
    if( iterator != unresolved_tiles.end()) {
        for_each_tile_in_bitmask(unresolved_tiles[position], [&](int index){ 
            total_weights += weights[index];
        });

        float rnd = (rand() % 1000 / 1000.0) * total_weights;

        for_each_tile_in_bitmask(unresolved_tiles[position], [&](int index){ 
            rnd -= weights[index];
            if(rnd < 0.0 && chosen == -1) {
                resolved_tiles[position] = tile_mask_reverse_index[index];
                chosen = index;
				std::cout << "Chosen: " << chosen << std::endl;
            }
        });

        // set mask temporarily for propagation
        int index = 0;
        for(auto& part : unresolved_tiles[position]) {
            part = 0;
            for(int i = 0; i < bits_per_segment; i++) {
                if(index * bits_per_segment + i == chosen)
                    part = pow(2, i);
            }
            index += 1;
        }
		_propagate(position);
		unresolved_tiles.erase(iterator);
	}
}

void WaveCollapse::_propagate(const Vector3i& position) {
    std::vector<Vector3i> stack;
    stack.push_back(position);

    while(!stack.empty()) {
        Vector3i cur_coords = stack.back();
        stack.pop_back();
        
        for(unsigned int d = 0; d < directions.size(); ++d) {
            Vector3i other_position = cur_coords + directions[d];
            if(other_position == position) { 
                continue;  // don't propagate back to original point
            }
            bool changed = false;

			if (unresolved_tiles.find(other_position) == unresolved_tiles.end()
				&& resolved_tiles.find(other_position) == resolved_tiles.end()) {
                // Other tile is not in resolved or unresolved - so add that tile to unresolved list
                _new_tile(other_position);
            }

            if(unresolved_tiles.find(other_position) != unresolved_tiles.end()) {
                auto unresolved = unresolved_tiles[cur_coords];
                std::vector<segment_type> valid_bitmask = {};
                for_each_tile_in_bitmask(unresolved_tiles[cur_coords], [&](int index){
                    // determine the valid bitmask for the other cell based on the 
                    // current cell's possible tiles.  Valid mask = OR of all valid masks
                    if(valid_bitmask.empty()) {
                        valid_bitmask = valid_combinations_mask[d][tile_mask_reverse_index[index]];
                    } else {
                        for(int i = 0; i < (int)valid_bitmask.size(); ++i) {
                            valid_bitmask[i] |= valid_combinations_mask[d][tile_mask_reverse_index[index]][i];
                        }
                    }
                });

                if(valid_bitmask.size() > 0) {
                    for(int i = 0; i < (int)unresolved_tiles[other_position].size(); ++i) {
                        // Do a bitwise AND with valid bitmask to see if the other cell tiles are valid
                        segment_type old_mask = unresolved_tiles[other_position][i];
                        segment_type new_mask = old_mask & valid_bitmask[i];
                        if(new_mask != old_mask) {
                            changed = true;
                            unresolved_tiles[other_position][i] = new_mask;
                        }
                    }
                }
            }

            if(changed) {
                stack.push_back(other_position);
            }
        }
    }
}

bool WaveCollapse::_within_radius(const Vector3i& position) {
    if(position.x*position.x + position.z*position.z <= radius_squared)
        return true;
    return false;
}

void WaveCollapse::_iterate() {
    Vector3i co_ords = _min_entropy_co_ords();
    _collapse(co_ords);
    emit_signal("cell_changed", co_ords, resolved_tiles[co_ords].tile, resolved_tiles[co_ords].rotation);
}

void WaveCollapse::process() {
    auto start_time = std::chrono::steady_clock::now();
    bool finished = false;

    while(!terminate_thread) {
        bool collapsed = true;
        {
            // lock to prevent main thread doesn't change player position
            const std::lock_guard<std::mutex> lock(g_mutex);
            for(auto const& tile : unresolved_tiles) {
                if(_within_radius(tile.first - player_position)) {
                    collapsed = false;
                    break;
                }
            }

            // resolve 1 tile
            if(!collapsed) {
                if(finished) {
                    start_time = std::chrono::steady_clock::now();
                    finished = false;
                }
                _iterate();
                std::cout << ".";
				//std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }
        }
        if(collapsed) {
            if(!finished) {
                finished = true;
                auto end_time = std::chrono::steady_clock::now();
                auto time_dur = std::chrono::duration<double>( end_time - start_time ).count();
                std::cout << std::endl << "WaveCollapse Time: " << std::to_string(time_dur) << std::endl;
            }

            // don't waste time when there is nothing to do
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void WaveCollapse::process_thread() {
    if(!g_future.valid()) {
        g_future = std::async(std::launch::async, &WaveCollapse::process, this);
    }
}

void WaveCollapse::_on_Player_position_changed(const Vector3i& position, const int& collapse_radius)
{
    if(position != player_position) {
        std::cout << "Position Changed" << std::endl;
        const std::lock_guard<std::mutex> lock(g_mutex);
        player_position = position;  
        this->radius = collapse_radius;
        this->radius_squared = radius * radius;
    }
    if(!setup_done) {
        _setup(0);
    }
}

void WaveCollapse::_setup(int y) {
    if(this->template_gridmap == nullptr) {
        this->template_gridmap = _path_to_object<GridMap>(this->template_gridmap_path);
    }
    // initialize tiles
    if(!setup_done) {
        generate_combinations();

        for(int x = -radius; x <= radius; ++x) {
            for(int z = -radius; z <= radius; ++z) {
                if(_within_radius(Vector3i(x,y,z))) {
                    _new_tile(Vector3i(x,y,z));
                }
            }
        }
    }
    setup_done = true;
}
