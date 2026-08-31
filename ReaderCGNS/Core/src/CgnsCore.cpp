#include "CgnsCore.h"
#include "CgnsTypes.hpp"
#include "Logger.h"

#include <algorithm>
#include <array>
#include <functional>
#include <unordered_set>
#include <vector>

#include "cgnslib.h"

void CgnsCore::info() const
{
    // Base Information
    int nbases = 0;
    CGNS_LOG_CALL(cg_nbases(this->get_file_id(), &nbases));
    for (int base = 1; base <= nbases; ++base) {
        char base_name[CGNS_NAME_MAX_LEN] = { }, base_biter_name[CGNS_NAME_MAX_LEN] = { };
        int base_cell_dim = 0, base_phys_dim = 0, base_iter_nsteps = 0, n1to1s_global = 0;
        CG_SimulationType_t base_simulation_type = CG_SimulationType_t::CG_SimulationTypeNull;
        CGNS_LOG_CALL(cg_base_read(this->get_file_id(), base, base_name, &base_cell_dim, &base_phys_dim));
        CGNS_LOG_CALL(cg_simulation_type_read(this->get_file_id(), base, &base_simulation_type));

        // Base Iterative Data
        CGNS_LOG_CALL(cg_biter_read(this->get_file_id(), base, base_biter_name, &base_iter_nsteps));

        // One-to-One Connectivity Global
        CGNS_LOG_CALL(cg_n1to1_global(this->get_file_id(), base, &n1to1s_global));

        LOG_INFO("[Base] {}:[{}] {}, CellDim={}, PhyDim={}, [{}] Iterative={}, n1to1s_global={}",
                 base,
                 cg_SimulationTypeName(base_simulation_type),
                 base_name,
                 base_cell_dim,
                 base_phys_dim,
                 base_biter_name,
                 base_iter_nsteps,
                 n1to1s_global);

        // Zone Information
        int nzones = 0;
        CGNS_LOG_CALL(cg_nzones(this->get_file_id(), base, &nzones));
        for (int zone = 1; zone <= nzones; ++zone) {
            char zone_name[CGNS_NAME_MAX_LEN] = { }, zone_iter_name[CGNS_NAME_MAX_LEN] = { };
            int zone_dim = 0;
            CG_ZoneType_t zone_type = CG_ZoneType_t::CG_ZoneTypeNull;
            std::array<cgsize_t, 9> zone_size { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

            CGNS_LOG_CALL(cg_index_dim(this->get_file_id(), base, zone, &zone_dim));
            CGNS_LOG_CALL(cg_zone_type(this->get_file_id(), base, zone, &zone_type));
            CGNS_LOG_CALL(cg_ziter_read(this->get_file_id(), base, zone, zone_iter_name));
            CGNS_LOG_CALL(cg_zone_read(this->get_file_id(), base, zone, zone_name, zone_size.data()));

            cgsize_t zone_vertex_sum = 0, zone_cell_sum = 0;
            if (zone_type == CG_ZoneType_t::CG_Structured) {
                switch (zone_dim) {
                case 2: {
                    zone_vertex_sum = zone_size[0] * zone_size[1];
                    zone_cell_sum = zone_size[2] * zone_size[3];
                    LOG_INFO("  [Zone]{:>2}:[{}] {}, Dim={}, NVertex=[{},{}]:{}, NCell=[{},{}]:{}, NBoundVertex=[{},{}], iter_name=[{}]",
                             zone,
                             cg_ZoneTypeName(zone_type),
                             zone_name,
                             zone_dim,
                             zone_size[0],
                             zone_size[1],
                             zone_vertex_sum,
                             zone_size[2],
                             zone_size[3],
                             zone_cell_sum,
                             zone_size[4],
                             zone_size[5],
                             zone_iter_name);

                } break;
                case 3: {
                    zone_vertex_sum = zone_size[0] * zone_size[1] * zone_size[2];
                    zone_cell_sum = zone_size[3] * zone_size[4] * zone_size[5];
                    LOG_INFO("  [Zone]{:>2}:[{}] {}, Dim={}, NVertex=[{},{},{}]:{}, NCell=[{},{},{}]:{}, NBoundVertex=[{},{},{}], iter_name=[{}]",
                             zone,
                             cg_ZoneTypeName(zone_type),
                             zone_name,
                             zone_dim,
                             zone_size[0],
                             zone_size[1],
                             zone_size[2],
                             zone_vertex_sum,
                             zone_size[3],
                             zone_size[4],
                             zone_size[5],
                             zone_cell_sum,
                             zone_size[6],
                             zone_size[7],
                             zone_size[8],
                             zone_iter_name);
                } break;
                default: {
                    LOG_WARN("  [Zone]{:>2}:[{}] {}, Invalid-Dim={}, iter_name=[{}]", zone, cg_ZoneTypeName(zone_type), zone_name, zone_dim, zone_iter_name);
                } break;
                }
            }
            else if (zone_type == CG_ZoneType_t::CG_Unstructured) {
                zone_vertex_sum = zone_size[0];
                zone_cell_sum = zone_size[1];

                LOG_INFO("  [Zone]{:>2}:[{}] {}, NVertex={}, NCell={}, NBoundVertex={}, iter_name=[{}]",
                         zone,
                         cg_ZoneTypeName(zone_type),
                         zone_name,
                         zone_vertex_sum,
                         zone_cell_sum,
                         zone_size[2],
                         zone_iter_name);
            }

            // Flow Solution
            int nsols = 0;
            CGNS_LOG_CALL(cg_nsols(this->get_file_id(), base, zone, &nsols));
            for (int sol = 1; sol <= nsols; ++sol) {
                char sol_name[CGNS_NAME_MAX_LEN] = { };
                int sol_data_dim = 0, sol_nfields = 0;
                std::vector<cgsize_t> sol_npnts(1, 0);
                std::vector<cgsize_t> sol_dim_vals(zone_dim, 0);
                CG_GridLocation_t sol_location = CG_GridLocation_t::CG_GridLocationNull;
                CG_PointSetType_t sol_point_set_type = CG_PointSetType_t::CG_PointSetTypeNull;
                CGNS_LOG_CALL(cg_nfields(this->get_file_id(), base, zone, sol, &sol_nfields));
                CGNS_LOG_CALL(cg_sol_info(this->get_file_id(), base, zone, sol, sol_name, &sol_location));
                CGNS_LOG_CALL(cg_sol_size(this->get_file_id(), base, zone, sol, &sol_data_dim, sol_dim_vals.data()));
                CGNS_LOG_CALL(cg_sol_ptset_info(this->get_file_id(), base, zone, sol, &sol_point_set_type, sol_npnts.data()));

                if (sol_point_set_type != CG_PointSetType_t::CG_PointSetTypeNull) {
                    sol_npnts.resize(sol_npnts[0] * zone_dim, 0);
                    CGNS_LOG_CALL(cg_sol_ptset_read(this->get_file_id(), base, zone, sol, sol_npnts.data()));
                }

                LOG_INFO("    [FlowSolution]{:>2}:[{}]-[{}] {}, NField={}, DataDim={}, DataVal={}, npnts={}",
                         sol,
                         cg_GridLocationName(sol_location),
                         cg_PointSetTypeName(sol_point_set_type),
                         sol_name,
                         sol_nfields,
                         sol_data_dim,
                         sol_dim_vals,
                         sol_npnts);
            }

            // Discrete Data
            int ndiscrete = 0;
            CGNS_LOG_CALL(cg_ndiscrete(this->get_file_id(), base, zone, &ndiscrete));
            for (int discrete = 1; discrete <= ndiscrete; ++discrete) {
                char discrete_name[CGNS_NAME_MAX_LEN] = { };
                int discrete_data_dim = 0;
                std::vector<cgsize_t> discrete_dim_vals(zone_dim, 0), discrete_npnts(1, 0);
                CG_PointSetType_t discrete_point_set_type = CG_PointSetType_t::CG_PointSetTypeNull;

                CGNS_LOG_CALL(cg_discrete_read(this->get_file_id(), base, zone, discrete, discrete_name));
                CGNS_LOG_CALL(cg_discrete_size(this->get_file_id(), base, zone, discrete, &discrete_data_dim, discrete_dim_vals.data()));
                CGNS_LOG_CALL(cg_discrete_ptset_info(this->get_file_id(), base, zone, discrete, &discrete_point_set_type, discrete_npnts.data()));

                if (discrete_point_set_type != CG_PointSetType_t::CG_PointSetTypeNull) {
                    discrete_npnts.resize(discrete_npnts[0] * zone_dim, 0);
                    CGNS_LOG_CALL(cg_discrete_ptset_read(this->get_file_id(), base, zone, discrete, discrete_npnts.data()));
                }

                LOG_INFO("    [DiscreteData]{:>2}:[{}] {}, DataDim={}, DimVal={}, npnts={}",
                         discrete,
                         cg_PointSetTypeName(discrete_point_set_type),
                         discrete_name,
                         discrete_data_dim,
                         discrete_dim_vals,
                         discrete_npnts);
            }

            // Zone Subregions
            int nsubregs = 0;
            CGNS_LOG_CALL(cg_nsubregs(this->get_file_id(), base, zone, &nsubregs));
            for (int subreg = 1; subreg <= nsubregs; ++subreg) {
                char subreg_name[CGNS_NAME_MAX_LEN] = { };
                int subreg_dim = 0, subreg_bcname_len = 0, subreg_gcname_len = 0;
                std::vector<cgsize_t> subreg_npnts(1, 0);
                CG_GridLocation_t subreg_location = CG_GridLocation_t::CG_GridLocationNull;
                CG_PointSetType_t subreg_point_set_type = CG_PointSetType_t::CG_PointSetTypeNull;

                CGNS_LOG_CALL(cg_subreg_info(this->get_file_id(),
                                       base,
                                       zone,
                                       subreg,
                                       subreg_name,
                                       &subreg_dim,
                                       &subreg_location,
                                       &subreg_point_set_type,
                                       subreg_npnts.data(),
                                       &subreg_bcname_len,
                                       &subreg_gcname_len));
                if (subreg_point_set_type != CG_PointSetType_t::CG_PointSetTypeNull) {
                    subreg_npnts.resize(subreg_npnts[0] * zone_dim, 1);
                    CGNS_LOG_CALL(cg_subreg_ptset_read(this->get_file_id(), base, zone, subreg, subreg_npnts.data()));
                }

                std::string msg = std::format("    [ZoneSubregions]{:>2}:[{}]-[{}] {}, Dimension={}",
                                              subreg,
                                              cg_GridLocationName(subreg_location),
                                              cg_PointSetTypeName(subreg_point_set_type),
                                              subreg_name,
                                              subreg_dim);
                if (subreg_bcname_len > 0) {
                    std::string subreg_bcname(subreg_bcname_len + 1, '\0');
                    CGNS_LOG_CALL(cg_subreg_bcname_read(this->get_file_id(), base, zone, subreg, subreg_bcname.data()));
                    msg += std::format(" baname={}", subreg_name);
                }
                if (subreg_gcname_len > 0) {
                    std::string subreg_gcname(subreg_gcname_len + 1, '\0');
                    CGNS_LOG_CALL(cg_subreg_gcname_read(this->get_file_id(), base, zone, subreg, subreg_gcname.data()));
                    msg += std::format(" gcname={}", subreg_name);
                }
                LOG_INFO("{} npnts={}", msg, subreg_npnts);
            }

            // Zone Grid Coordinates
            int ngrids = 0;
            CGNS_LOG_CALL(cg_ngrids(this->get_file_id(), base, zone, &ngrids));
            for (int grid = 1; grid <= ngrids; ++grid) {
                char grid_name[CGNS_NAME_MAX_LEN] = { };
                // CG_DataType_t grid_data_type = CG_DataType_t::CG_DataTypeNull;
                // std::vector<cgsize_t> grid_bounding_box(1, 0);

                CGNS_LOG_CALL(cg_grid_read(this->get_file_id(), base, zone, grid, grid_name));
                // CG_INFO(cg_grid_bounding_box_read(this->GetFileID(), base, zone, grid, grid_data_type, grid_bounding_box.data()));

                LOG_INFO("    [ZoneGird]{:>2}:[{}]", grid, grid_name);
            }
            int ncoords = 0;
            std::string coord_info_msg;
            CGNS_LOG_CALL(cg_ncoords(this->get_file_id(), base, zone, &ncoords));
            for (int coord = 1; coord <= ncoords; ++coord) {
                char coord_name[CGNS_NAME_MAX_LEN] = { };
                CG_DataType_t coord_data_type = CG_DataType_t::CG_DataTypeNull;
                CGNS_LOG_CALL(cg_coord_info(this->get_file_id(), base, zone, coord, &coord_data_type, coord_name));

                coord_info_msg += std::format("[{}-{}-{}], ", coord, cg_DataTypeName(coord_data_type), coord_name);
            }
            if (!coord_info_msg.empty()) {
                coord_info_msg.erase(coord_info_msg.size() - 2);
                LOG_INFO("    [ZoneCoords] {}", coord_info_msg);
            }

            // Element Connectivity
            int nsections = 0;
            CGNS_LOG_CALL(cg_nsections(this->get_file_id(), base, zone, &nsections));
            for (int section = 1; section <= nsections; ++section) {
                char section_name[CGNS_NAME_MAX_LEN] = { };
                CG_ElementType_t section_element_type = CG_ElementType_t::CG_ElementTypeNull;
                cgsize_t section_start = 0, section_end = 0;
                int section_nbndry = 0, section_parent_flag = 0;
                CGNS_LOG_CALL(cg_section_read(this->get_file_id(),
                                        base,
                                        zone,
                                        section,
                                        section_name,
                                        &section_element_type,
                                        &section_start,
                                        &section_end,
                                        &section_nbndry,
                                        &section_parent_flag));
                if (section_end == 0 || section_end - section_start < 0) {
                    LOG_INFO("    [ZoneSection] {} element range [start, end] is empty.", section_name);
                    continue;
                }

                cgsize_t element_data_size = 0;
                CGNS_LOG_CALL(cg_ElementDataSize(this->get_file_id(), base, zone, section, &element_data_size));
                if (element_data_size <= 0) {
                    LOG_INFO("    [ZoneSection] {} element data is empty.", section_name);
                    continue;
                }

                std::vector<cgsize_t> elements(element_data_size, 0);
                cgsize_t section_element_sum = section_end - section_start + 1;
                static const std::unordered_set MIX_ELEMENT = { CG_ElementType_t::CG_MIXED, CG_ElementType_t::CG_NGON_n, CG_ElementType_t::CG_NFACE_n };
                if (MIX_ELEMENT.contains(section_element_type)) {
                    std::vector<cgsize_t> elements_connect_offset(section_element_sum + 1, 0);
                    CGNS_LOG_CALL(cg_poly_elements_read(this->get_file_id(), base, zone, section, elements.data(), elements_connect_offset.data(), nullptr));
                }
                else {
                    CGNS_LOG_CALL(cg_elements_read(this->get_file_id(), base, zone, section, elements.data(), nullptr));
                }

                if (elements.empty()) {
                    LOG_INFO("    [ElementConnectivity]{:>2}:[{}] {}, ElementRange=<empty>:{}",
                             section,
                             cg_ElementTypeName(section_element_type),
                             section_name,
                             section_element_sum);
                }
                else {
                    auto by_abs = [](auto value) {
                        return std::abs(value);
                    };
                    LOG_INFO("    [ElementConnectivity]{:>2}:[{}] {}, ElementRange=[{},{}]:{}",
                             section,
                             cg_ElementTypeName(section_element_type),
                             section_name,
                             std::ranges::min(elements, std::ranges::less { }, by_abs),
                             std::ranges::max(elements, std::ranges::less { }, by_abs),
                             section_element_sum);
                }
            }

            // One-to-One Connectivity
            int n1to1s = 0;
            CGNS_LOG_CALL(cg_n1to1(this->get_file_id(), base, zone, &n1to1s));
            for (int n1to1 = 1; n1to1 <= n1to1s; ++n1to1) {
                char n1to1_connectname[CGNS_NAME_MAX_LEN] = { };
                char n1to1_donorname[CGNS_NAME_MAX_LEN] = { };
                std::vector<cgsize_t> n1to1_range(zone_dim * 2, 0), donor_range(zone_dim * 2, 0);
                std::vector<int> n1to1_transform(zone_dim, 0);

                CGNS_LOG_CALL(cg_1to1_read(this->get_file_id(),
                                     base,
                                     zone,
                                     n1to1,
                                     n1to1_connectname,
                                     n1to1_donorname,
                                     n1to1_range.data(),
                                     donor_range.data(),
                                     n1to1_transform.data()));
                LOG_INFO("    [One-to-One Connectivity]{:>2}:[{}] {}, range={}, donor_range={}, transform={}",
                         n1to1,
                         n1to1_connectname,
                         n1to1_donorname,
                         n1to1_range,
                         donor_range,
                         n1to1_transform);
            }

            // Generalized Connectivity
            int nconns = 0;
            CGNS_LOG_CALL(cg_nconns(this->get_file_id(), base, zone, &nconns));
            for (int ncoon = 1; ncoon <= nconns; ++ncoon) {
                cgsize_t npnts = 0, ndata_donor = 0;
                char ncoon_name[CGNS_NAME_MAX_LEN] = { }, ncoon_donor_name[CGNS_NAME_MAX_LEN] = { };
                CG_GridLocation_t ncoon_loc = CG_GridLocation_t::CG_GridLocationNull;
                CG_GridConnectivityType_t connect_type = CG_GridConnectivityType_t::CG_GridConnectivityTypeNull;
                CG_PointSetType_t ptset_type = CG_PointSetType_t::CG_PointSetTypeNull, donor_ptset_type = CG_PointSetType_t::CG_PointSetTypeNull;
                CG_ZoneType_t donor_zonetype = CG_ZoneType_t::CG_ZoneTypeNull;
                CG_DataType_t donor_datatype = CG_DataType_t::CG_DataTypeNull;

                CGNS_LOG_CALL(cg_conn_info(this->get_file_id(),
                                     base,
                                     zone,
                                     ncoon,
                                     ncoon_name,
                                     &ncoon_loc,
                                     &connect_type,
                                     &ptset_type,
                                     &npnts,
                                     ncoon_donor_name,
                                     &donor_zonetype,
                                     &donor_ptset_type,
                                     &donor_datatype,
                                     &ndata_donor));

                LOG_INFO("    [GeneralizedConnectivity]{:>2}:[{}]-[{}] {}, [{}]:{}, [{}]-[{}] {}, [{}]:{}",
                         ncoon,
                         cg_GridLocationName(ncoon_loc),
                         cg_GridConnectivityTypeName(connect_type),
                         ncoon_name,
                         cg_PointSetTypeName(ptset_type),
                         npnts,
                         cg_ZoneTypeName(donor_zonetype),
                         cg_PointSetTypeName(donor_ptset_type),
                         cg_DataTypeName(donor_datatype),
                         ncoon_donor_name,
                         ndata_donor);
            }
            int nholes = 0;
            CGNS_LOG_CALL(cg_nholes(this->get_file_id(), base, zone, &nholes));
            for (int hole = 1; hole <= nholes; ++hole) {
                char hole_name[CGNS_NAME_MAX_LEN] = { };
                CG_GridLocation_t hole_location = CG_GridLocation_t::CG_GridLocationNull;
                CG_PointSetType_t hole_ptset_type = CG_PointSetType_t::CG_PointSetTypeNull;
                int hole_nptsets = 0;
                cgsize_t hole_npnts = 0;

                CGNS_LOG_CALL(cg_hole_info(this->get_file_id(), base, zone, hole, hole_name, &hole_location, &hole_ptset_type, &hole_nptsets, &hole_npnts));

                LOG_INFO("    [GeneralizedConnectivity]{:>2}:[{}] {}, [{}] nptsets={}, npnts={}",
                         hole,
                         cg_GridLocationName(hole_location),
                         hole_name,
                         cg_PointSetTypeName(hole_ptset_type),
                         hole_nptsets,
                         hole_npnts);
            }

            // Boundary Conditions
            int nbocos = 0;
            CGNS_LOG_CALL(cg_nbocos(this->get_file_id(), base, zone, &nbocos));
            for (int boco = 1; boco <= nbocos; ++boco) {
                char boco_name[CGNS_NAME_MAX_LEN] = { };
                CG_BCType_t boco_type = CG_BCType_t::CG_BCTypeNull;
                CG_PointSetType_t boco_ptset_type = CG_PointSetType_t::CG_PointSetTypeNull;
                CG_DataType_t boco_normal_datatype = CG_DataType_t::CG_DataTypeNull;
                CG_GridLocation_t boco_location = CG_GridLocation_t::CG_GridLocationNull;
                cgsize_t boco_npnts = 0, boco_normal_list_size = 0;
                std::vector<int> boco_normal_index(zone_dim, 0);
                int boco_ndataset = 0;

                CGNS_LOG_CALL(cg_boco_gridlocation_read(this->get_file_id(), base, zone, boco, &boco_location));
                CGNS_LOG_CALL(cg_boco_info(this->get_file_id(),
                                     base,
                                     zone,
                                     boco,
                                     boco_name,
                                     &boco_type,
                                     &boco_ptset_type,
                                     &boco_npnts,
                                     boco_normal_index.data(),
                                     &boco_normal_list_size,
                                     &boco_normal_datatype,
                                     &boco_ndataset));

                LOG_INFO("    [BoundaryConditions]{:>2}:[{}]-[{}] {}, [{}] npnts={}, [{}] index={}, listSize={}, ndataset={}",
                         boco,
                         cg_BCTypeName(boco_type),
                         cg_GridLocationName(boco_location),
                         boco_name,
                         cg_PointSetTypeName(boco_ptset_type),
                         boco_npnts,
                         cg_DataTypeName(boco_normal_datatype),
                         boco_normal_index,
                         boco_normal_list_size,
                         boco_ndataset);

                for (int boco_dataset = 1; boco_dataset <= boco_ndataset; ++boco_dataset) {
                    char boco_dataset_name[CGNS_NAME_MAX_LEN] = { };
                    CG_BCType_t boco_dataset_type = CG_BCType_t::CG_BCTypeNull;
                    int boco_dataset_dirichlet_flag = 0, boco_dataset_neumann_flag = 0;

                    CGNS_LOG_CALL(cg_dataset_read(this->get_file_id(),
                                            base,
                                            zone,
                                            boco,
                                            boco_dataset,
                                            boco_dataset_name,
                                            &boco_dataset_type,
                                            &boco_dataset_dirichlet_flag,
                                            &boco_dataset_neumann_flag));

                    LOG_INFO("      [BoundaryConditionsDataset]{:>2}:[{}]-[{}] {}, DirichletFlag={}, NeumannFlag ={}",
                             boco_dataset,
                             cg_BCTypeName(boco_dataset_type),
                             cg_GridLocationName(boco_location),
                             boco_dataset_name,
                             boco_dataset_dirichlet_flag,
                             boco_dataset_neumann_flag);
                }
            }

            // Rigid Grid Motion
            int n_rigid_motions = 0;
            CGNS_LOG_CALL(cg_n_rigid_motions(this->get_file_id(), base, zone, &n_rigid_motions));
            for (int rigid_motion = 1; rigid_motion <= n_rigid_motions; ++rigid_motion) {
                char rigid_motion_name[CGNS_NAME_MAX_LEN] = { };
                CG_RigidGridMotionType_t rigid_motion_type = CG_RigidGridMotionType_t::CG_RigidGridMotionTypeNull;
                CGNS_LOG_CALL(cg_rigid_motion_read(this->get_file_id(), base, zone, rigid_motion, rigid_motion_name, &rigid_motion_type));

                LOG_INFO("    [RigidGridMotion]{:>2}:[{}] {}", rigid_motion, cg_RigidGridMotionTypeName(rigid_motion_type), rigid_motion_name);
            }

            // Arbitrary Grid Motion
            int n_arbitrary_motions = 0;
            CGNS_LOG_CALL(cg_n_arbitrary_motions(this->get_file_id(), base, zone, &n_arbitrary_motions));
            for (int arbitrary_motion = 1; arbitrary_motion <= n_arbitrary_motions; ++arbitrary_motion) {
                char arbitrary_motion_name[CGNS_NAME_MAX_LEN] = { };
                CG_ArbitraryGridMotionType_t arbitrary_motion_type = CG_ArbitraryGridMotionType_t::CG_ArbitraryGridMotionTypeNull;
                CGNS_LOG_CALL(cg_arbitrary_motion_read(this->get_file_id(), base, zone, arbitrary_motion, arbitrary_motion_name, &arbitrary_motion_type));

                LOG_INFO("    [ArbitraryGridMotion]{:>2}:[{}] {}", arbitrary_motion, cg_ArbitraryGridMotionTypeName(arbitrary_motion_type), arbitrary_motion_name);
            }

            // Zone Grid Connectivity
            int nzconns = 0;
            CGNS_LOG_CALL(cg_nzconns(this->get_file_id(), base, zone, &nzconns));
            for (int zconn = 1; zconn <= nzconns; ++zconn) {
                char zconn_name[CGNS_NAME_MAX_LEN] = { };
                CGNS_LOG_CALL(cg_zconn_read(this->get_file_id(), base, zone, zconn, zconn_name));

                LOG_INFO("    [ZoneGridConnectivity]{:>2}:[NULL] {}", zconn, zconn_name);
            }
        }

        // Particle Zone Information
        int nparticlezones = 0;
        CGNS_LOG_CALL(cg_nparticle_zones(this->get_file_id(), base, &nparticlezones));
        for (int particle_zone = 1; particle_zone <= nparticlezones; ++particle_zone) {
            double particle_zone_id = 0;
            char particle_zone_name[CGNS_NAME_MAX_LEN] = { };
            cgsize_t particle_zone_size = 0;

            CGNS_LOG_CALL(cg_particle_id(this->get_file_id(), base, particle_zone, &particle_zone_id));
            CGNS_LOG_CALL(cg_particle_read(this->get_file_id(), base, particle_zone, particle_zone_name, &particle_zone_size));

            LOG_INFO("  [Particle]{:>2}:[NULL] {}, id={}, size={}", particle_zone, particle_zone_name, particle_zone_id, particle_zone_size);

            // Particle Coordinates
            int particle_ncoord_nodes = 0;
            CGNS_LOG_CALL(cg_particle_ncoord_nodes(this->get_file_id(), base, particle_zone, &particle_ncoord_nodes));
            for (int particle_zone_coord = 1; particle_zone_coord <= particle_ncoord_nodes; ++particle_zone_coord) {
                char particle_zone_coord_name[CGNS_NAME_MAX_LEN] = { };
                CG_DataType_t particle_zone_coord_type = CG_DataType_t::CG_DataTypeNull;

                CGNS_LOG_CALL(cg_particle_coord_info(this->get_file_id(), base, particle_zone, particle_zone_coord, &particle_zone_coord_type, particle_zone_coord_name));

                LOG_INFO("    [ParticleCoordinates]{:>2}:[{}] {}", particle_zone_coord, cg_DataTypeName(particle_zone_coord_type), particle_zone_coord_name);
            }

            // Particle Solution
            int particle_nsols = 0;
            CGNS_LOG_CALL(cg_particle_nsols(this->get_file_id(), base, particle_zone, &particle_nsols));
            for (int particle_sol = 1; particle_sol <= particle_nsols; ++particle_sol) {
                char particle_sol_name[CGNS_NAME_MAX_LEN] = { };
                double particle_sol_id = 0.0;
                cgsize_t particle_sol_size = 0, particle_sol_npnts = 0;
                std::vector<cgsize_t> particle_sol_pnts;
                CG_PointSetType_t particle_sol_ptset_type = CG_PointSetType_t::CG_PointSetTypeNull;

                CGNS_LOG_CALL(cg_particle_sol_info(this->get_file_id(), base, particle_zone, particle_sol, particle_sol_name));
                CGNS_LOG_CALL(cg_particle_sol_id(this->get_file_id(), base, particle_zone, particle_sol, &particle_sol_id));
                CGNS_LOG_CALL(cg_particle_sol_size(this->get_file_id(), base, particle_zone, particle_sol, &particle_sol_size));
                CGNS_LOG_CALL(cg_particle_sol_ptset_info(this->get_file_id(), base, particle_zone, particle_sol, &particle_sol_ptset_type, &particle_sol_npnts));

                if (particle_sol_npnts != 0) {
                    particle_sol_pnts.resize(particle_sol_npnts);
                    CGNS_LOG_CALL(cg_particle_sol_ptset_read(this->get_file_id(), base, particle_zone, particle_sol, particle_sol_pnts.data()));
                }

                int particle_nfields = 0;
                CGNS_LOG_CALL(cg_particle_nfields(this->get_file_id(), base, particle_zone, particle_sol, &particle_nfields));
                LOG_INFO("    [ParticleSolution]{:>2}:[{}] {}, id={}, size={}, npnts={}, pnts={}, nfields={}",
                         particle_sol,
                         cg_PointSetTypeName(particle_sol_ptset_type),
                         particle_sol_name,
                         particle_sol_id,
                         particle_sol_size,
                         particle_sol_npnts,
                         particle_sol_pnts,
                         particle_nfields);
            }
        }
    }
}
