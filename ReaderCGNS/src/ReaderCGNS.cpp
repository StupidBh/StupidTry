#include "ReaderCGNS/ReaderCGNS.h"
#include "Logger.h"

#include <set>
#include <vector>
#include <array>

#include "cgnslib.h"

namespace ReaderCGNS {
    namespace Logger {
        void SetLogCallback(ReaderCGNS_LogCallback callback)
        {
            g_log_callback.store(callback, std::memory_order_release);
        }

        void ClearLogCallback()
        {
            g_log_callback.store(nullptr, std::memory_order_release);
        }
    }

    bool info(const std::string& cgns_file_path)
    {
        LOG_INFO("Open in read only: [{}]", cgns_file_path);

        int cgns_file_type = -1;
        const int status = cg_is_cgns(cgns_file_path.c_str(), &cgns_file_type);
        auto FileTypeName = [](int file_type) -> const char* {
            switch (file_type) {
            case CG_FILE_ADF : return "ADF";
            case CG_FILE_ADF2: return "ADF2";
            case CG_FILE_HDF5: return "HDF5";
            default          : return "ERROR_FILE";
            }
        };
        if (status != CG_OK || cgns_file_type == CG_FILE_NONE) {
            LOG_INFO("[CG_ERROR] [{}] msg: {}", FileTypeName(cgns_file_type), cg_get_error());
            return false;
        }

        int cg_file_id = 0;
        if (CG_INFO(cg_open(cgns_file_path.c_str(), CG_MODE_READ, &cg_file_id)) != CG_OK) {
            return false;
        }

        float cg_file_version = 0.F;
        int cg_file_precision = 0;
        CG_INFO(cg_version(cg_file_id, &cg_file_version));
        CG_INFO(cg_precision(cg_file_id, &cg_file_precision));
        LOG_INFO("[{}] v{:.2f}, precision={}, file_id={}", FileTypeName(cgns_file_type), cg_file_version, cg_file_precision, cg_file_id);

        // Base Information
        int nbases = 0;
        CG_INFO(cg_nbases(cg_file_id, &nbases));
        for (int base = 1; base <= nbases; ++base) {
            std::string base_name(33, '\0'), base_biter_name(33, '\0');
            int base_cell_dim = 0, base_phys_dim = 0, base_iter_nsteps = 0, n1to1s_global = 0;
            CG_SimulationType_t base_simulation_type = CG_SimulationType_t::CG_SimulationTypeNull;
            CG_INFO(cg_base_read(cg_file_id, base, base_name.data(), &base_cell_dim, &base_phys_dim));
            CG_INFO(cg_simulation_type_read(cg_file_id, base, &base_simulation_type));

            // Base Iterative Data
            CG_INFO(cg_biter_read(cg_file_id, base, base_biter_name.data(), &base_iter_nsteps));

            // One-to-One Connectivity Global
            CG_INFO(cg_n1to1_global(cg_file_id, base, &n1to1s_global));

            LOG_INFO("[Base] {}:[{}] {}, CellDim={}, PhyDim={}, [{}] Iterative={}, n1to1s_global={}",
                     base,
                     cg_SimulationTypeName(base_simulation_type),
                     base_name.c_str(),
                     base_cell_dim,
                     base_phys_dim,
                     (base_biter_name.empty() ? "NULL" : base_biter_name.c_str()),
                     base_iter_nsteps,
                     n1to1s_global);

            // Zone Information
            int nzones = 0;
            CG_INFO(cg_nzones(cg_file_id, base, &nzones));
            for (int zone = 1; zone <= nzones; ++zone) {
                std::string zone_name(33, '\0'), zone_iter_name(33, '\0');
                int zone_dim = 0;
                CG_ZoneType_t zone_type = CG_ZoneType_t::CG_ZoneTypeNull;
                std::array<cgsize_t, 9> zone_size { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

                CG_INFO(cg_index_dim(cg_file_id, base, zone, &zone_dim));
                CG_INFO(cg_zone_type(cg_file_id, base, zone, &zone_type));
                CG_INFO(cg_ziter_read(cg_file_id, base, zone, zone_iter_name.data()));
                CG_INFO(cg_zone_read(cg_file_id, base, zone, zone_name.data(), zone_size.data()));

                cgsize_t zone_vertex_sum = 0, zone_cell_sum = 0;
                if (zone_type == CG_ZoneType_t::CG_Structured) {
                    switch (zone_dim) {
                    case 2: {
                        zone_vertex_sum = zone_size[0] * zone_size[1];
                        zone_cell_sum = zone_size[2] * zone_size[3];
                        LOG_INFO("  [Zone]{:>2}:[{}] {}, Dim={}, NVertex=[{},{}]:{}, NCell=[{},{}]:{}, NBoundVertex=[{},{}], iter_name=[{}]",
                                 zone,
                                 cg_ZoneTypeName(zone_type),
                                 zone_name.c_str(),
                                 zone_dim,
                                 zone_size[0],
                                 zone_size[1],
                                 zone_vertex_sum,
                                 zone_size[2],
                                 zone_size[3],
                                 zone_cell_sum,
                                 zone_size[4],
                                 zone_size[5],
                                 (zone_iter_name.empty() ? "NULL" : zone_iter_name.c_str()));

                    } break;
                    case 3: {
                        zone_vertex_sum = zone_size[0] * zone_size[1] * zone_size[2];
                        zone_cell_sum = zone_size[3] * zone_size[4] * zone_size[5];
                        LOG_INFO("  [Zone]{:>2}:[{}] {}, Dim={}, NVertex=[{},{},{}]:{}, NCell=[{},{},{}]:{}, NBoundVertex=[{},{},{}], iter_name=[{}]",
                                 zone,
                                 cg_ZoneTypeName(zone_type),
                                 zone_name.c_str(),
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
                                 (zone_iter_name.empty() ? "NULL" : zone_iter_name.c_str()));
                    } break;
                    default: {
                        LOG_WARN("  [Zone]{:>2}:[{}] {}, Invalid-Dim={}, iter_name=[{}]",
                                 zone,
                                 cg_ZoneTypeName(zone_type),
                                 zone_name.c_str(),
                                 zone_dim,
                                 (zone_iter_name.empty() ? "NULL" : zone_iter_name.c_str()));
                    } break;
                    }
                }
                else if (zone_type == CG_ZoneType_t::CG_Unstructured) {
                    zone_vertex_sum = zone_size[0];
                    zone_cell_sum = zone_size[1];

                    LOG_INFO("  [Zone]{:>2}:[{}] {}, NVertex={}, NCell={}, NBoundVertex={}, iter_name=[{}]",
                             zone,
                             cg_ZoneTypeName(zone_type),
                             zone_name.c_str(),
                             zone_vertex_sum,
                             zone_cell_sum,
                             zone_size[2],
                             (zone_iter_name.empty() ? "NULL" : zone_iter_name.c_str()));
                }

                // Flow Solution
                int nsols = 0;
                CG_INFO(cg_nsols(cg_file_id, base, zone, &nsols));
                for (int sol = 1; sol <= nsols; ++sol) {
                    std::string sol_name(33, '\0');
                    int sol_data_dim = 0, sol_nfields = 0;
                    std::vector<cgsize_t> sol_npnts(1, 0);
                    std::vector<cgsize_t> sol_dim_vals(zone_dim, 0);
                    CG_GridLocation_t sol_location = CG_GridLocation_t::CG_GridLocationNull;
                    CG_PointSetType_t sol_point_set_type = CG_PointSetType_t::CG_PointSetTypeNull;
                    CG_INFO(cg_nfields(cg_file_id, base, zone, sol, &sol_nfields));
                    CG_INFO(cg_sol_info(cg_file_id, base, zone, sol, sol_name.data(), &sol_location));
                    CG_INFO(cg_sol_size(cg_file_id, base, zone, sol, &sol_data_dim, sol_dim_vals.data()));
                    CG_INFO(cg_sol_ptset_info(cg_file_id, base, zone, sol, &sol_point_set_type, sol_npnts.data()));

                    if (sol_point_set_type != CG_PointSetType_t::CG_PointSetTypeNull) {
                        sol_npnts.resize(sol_npnts[0] * zone_dim, 0);
                        CG_INFO(cg_sol_ptset_read(cg_file_id, base, zone, sol, sol_npnts.data()));
                    }

                    LOG_INFO("    [FlowSolution]{:>2}:[{}]-[{}] {}, NField={}, DataDim={}, DataVal={}, npnts={}",
                             sol,
                             cg_GridLocationName(sol_location),
                             cg_PointSetTypeName(sol_point_set_type),
                             sol_name.c_str(),
                             sol_nfields,
                             sol_data_dim,
                             sol_dim_vals,
                             sol_npnts);
                }

                // Discrete Data
                int ndiscrete = 0;
                CG_INFO(cg_ndiscrete(cg_file_id, base, zone, &ndiscrete));
                for (int discrete = 1; discrete <= ndiscrete; ++discrete) {
                    std::string discrete_name(33, '\0');
                    int discrete_data_dim = 0;
                    std::vector<cgsize_t> discrete_dim_vals(zone_dim, 0), discrete_npnts(1, 0);
                    CG_PointSetType_t discrete_point_set_type = CG_PointSetType_t::CG_PointSetTypeNull;

                    CG_INFO(cg_discrete_read(cg_file_id, base, zone, discrete, discrete_name.data()));
                    CG_INFO(cg_discrete_size(cg_file_id, base, zone, discrete, &discrete_data_dim, discrete_dim_vals.data()));
                    CG_INFO(cg_discrete_ptset_info(cg_file_id, base, zone, discrete, &discrete_point_set_type, discrete_npnts.data()));

                    if (discrete_point_set_type != CG_PointSetType_t::CG_PointSetTypeNull) {
                        discrete_npnts.resize(discrete_npnts[0] * zone_dim, 0);
                        CG_INFO(cg_discrete_ptset_read(cg_file_id, base, zone, discrete, discrete_npnts.data()));
                    }

                    LOG_INFO("    [DiscreteData]{:>2}:[{}] {}, DataDim={}, DimVal={}, npnts={}",
                             discrete,
                             cg_PointSetTypeName(discrete_point_set_type),
                             discrete_name.c_str(),
                             discrete_data_dim,
                             discrete_dim_vals,
                             discrete_npnts);
                }

                // Zone Subregions
                int nsubregs = 0;
                CG_INFO(cg_nsubregs(cg_file_id, base, zone, &nsubregs));
                for (int subreg = 1; subreg <= nsubregs; ++subreg) {
                    std::string subreg_name(33, '\0');
                    int subreg_dim = 0, subreg_bcname_len = 0, subreg_gcname_len;
                    std::vector<cgsize_t> subreg_npnts(1, 0);
                    CG_GridLocation_t subreg_location = CG_GridLocation_t::CG_GridLocationNull;
                    CG_PointSetType_t subreg_point_set_type = CG_PointSetType_t::CG_PointSetTypeNull;

                    CG_INFO(cg_subreg_info(cg_file_id,
                                           base,
                                           zone,
                                           subreg,
                                           subreg_name.data(),
                                           &subreg_dim,
                                           &subreg_location,
                                           &subreg_point_set_type,
                                           subreg_npnts.data(),
                                           &subreg_bcname_len,
                                           &subreg_gcname_len));
                    if (subreg_point_set_type != CG_PointSetType_t::CG_PointSetTypeNull) {
                        subreg_npnts.resize(subreg_npnts[0] * zone_dim, 1);
                        CG_INFO(cg_subreg_ptset_read(cg_file_id, base, zone, subreg, subreg_npnts.data()));
                    }

                    std::string msg = fmt::format("    [ZoneSubregions]{:>2}:[{}]-[{}] {}, Dimension={}",
                                                  subreg,
                                                  cg_GridLocationName(subreg_location),
                                                  cg_PointSetTypeName(subreg_point_set_type),
                                                  subreg_name.c_str(),
                                                  subreg_dim);
                    if (subreg_bcname_len > 0) {
                        std::string subreg_bcname(subreg_bcname_len, '\0');
                        CG_INFO(cg_subreg_bcname_read(cg_file_id, base, zone, subreg, subreg_bcname.data()));
                        msg += fmt::format(" baname={}", subreg_bcname);
                    }
                    if (subreg_gcname_len > 0) {
                        std::string subreg_gcname(subreg_gcname_len, '\0');
                        CG_INFO(cg_subreg_gcname_read(cg_file_id, base, zone, subreg, subreg_gcname.data()));
                        msg += fmt::format(" gcname={}", subreg_gcname);
                    }
                    LOG_INFO("{} npnts={}", msg, subreg_npnts);
                }

                // Zone Grid Coordinates
                int ngrids = 0;
                CG_INFO(cg_ngrids(cg_file_id, base, zone, &ngrids));
                for (int grid = 1; grid <= ngrids; ++grid) {
                    std::string grid_name(33, '\0');
                    CG_DataType_t grid_data_type = CG_DataType_t::CG_DataTypeNull;
                    std::vector<cgsize_t> grid_boundingbox(1, 0);

                    CG_INFO(cg_grid_read(cg_file_id, base, zone, grid, grid_name.data()));
                    CG_INFO(cg_grid_bounding_box_read(cg_file_id, base, zone, grid, grid_data_type, grid_boundingbox.data()));

                    LOG_INFO("    [ZoneGird]{:>2}:[{}] {}", grid, cg_DataTypeName(grid_data_type), grid_name);
                }
                int ncoords = 0;
                CG_INFO(cg_ncoords(cg_file_id, base, zone, &ncoords));
                for (int coord = 1; coord <= ncoords; ++coord) {
                    std::string coord_name(33, '\0');
                    CG_DataType_t coord_data_type = CG_DataType_t::CG_DataTypeNull;
                    CG_INFO(cg_coord_info(cg_file_id, base, zone, coord, &coord_data_type, coord_name.data()));

                    LOG_INFO("    [ZoneCoords]{:>2}:[{}] {}", coord, cg_DataTypeName(coord_data_type), coord_name);
                }

                // Element Connectivity
                int nsections = 0;
                CG_INFO(cg_nsections(cg_file_id, base, zone, &nsections));
                for (int section = 1; section <= nsections; ++section) {
                    std::string section_name(33, '\0');
                    CG_ElementType_t section_element_type = CG_ElementType_t::CG_ElementTypeNull;
                    cgsize_t section_start = 0, section_end = 0;
                    int section_nbndry = 0, section_parent_flag = 0;
                    CG_INFO(cg_section_read(cg_file_id,
                                            base,
                                            zone,
                                            section,
                                            section_name.data(),
                                            &section_element_type,
                                            &section_start,
                                            &section_end,
                                            &section_nbndry,
                                            &section_parent_flag));

                    cgsize_t element_data_size = 0;
                    CG_INFO(cg_ElementDataSize(cg_file_id, base, zone, section, &element_data_size));

                    std::vector<cgsize_t> elements(element_data_size, 0);
                    cgsize_t section_element_sum = section_end - section_start + 1;
                    static const std::set MIX_ELEMENT = { CG_ElementType_t::CG_MIXED, CG_ElementType_t::CG_NGON_n, CG_ElementType_t::CG_NFACE_n };
                    if (MIX_ELEMENT.contains(section_element_type)) {
                        std::vector<cgsize_t> elements_connect_offset(section_element_sum + 1, 0);
                        CG_INFO(cg_poly_elements_read(cg_file_id, base, zone, section, elements.data(), elements_connect_offset.data(), nullptr));
                    }
                    else {
                        CG_INFO(cg_elements_read(cg_file_id, base, zone, section, elements.data(), nullptr));
                    }

                    if (elements.empty()) {
                        LOG_INFO("    [ElementConnectivity]{:>2}:[{}] {}, ElementRange=<empty>:{}",
                                 section,
                                 cg_ElementTypeName(section_element_type),
                                 section_name.c_str(),
                                 section_element_sum);
                    }
                    else {
                        auto by_abs = [](auto value) {
                            return std::abs(value);
                        };
                        LOG_INFO("    [ElementConnectivity]{:>2}:[{}] {}, ElementRange=[{},{}]:{}",
                                 section,
                                 cg_ElementTypeName(section_element_type),
                                 section_name.c_str(),
                                 std::ranges::min(elements, std::ranges::less { }, by_abs),
                                 std::ranges::max(elements, std::ranges::less { }, by_abs),
                                 section_element_sum);
                    }
                }

                // One-to-One Connectivity
                int n1to1s = 0;
                CG_INFO(cg_n1to1(cg_file_id, base, zone, &n1to1s));
                for (int n1to1 = 1; n1to1 <= n1to1s; ++n1to1) {
                    std::string n1to1_connectname(33, '\0');
                    std::string n1to1_donorname(33, '\0');
                    cgsize_t n1to1_range = 0, donor_range = 0;
                    int n1to1_transform = 0;

                    CG_INFO(cg_1to1_read(cg_file_id,
                                         base,
                                         zone,
                                         n1to1,
                                         n1to1_connectname.data(),
                                         n1to1_donorname.data(),
                                         &n1to1_range,
                                         &donor_range,
                                         &n1to1_transform));
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
                CG_INFO(cg_nconns(cg_file_id, base, zone, &nconns));
                for (int ncoon = 1; ncoon <= nconns; ++ncoon) {
                    cgsize_t npnts = 0, ndata_donor = 0;
                    std::string ncoon_name(33, '\0'), ncoon_donor_name(33, '\0');
                    CG_GridLocation_t ncoon_loc = CG_GridLocation_t::CG_GridLocationNull;
                    CG_GridConnectivityType_t connect_type = CG_GridConnectivityType_t::CG_GridConnectivityTypeNull;
                    CG_PointSetType_t ptset_type = CG_PointSetType_t::CG_PointSetTypeNull, donor_ptset_type = CG_PointSetType_t::CG_PointSetTypeNull;
                    CG_ZoneType_t donor_zonetype = CG_ZoneType_t::CG_ZoneTypeNull;
                    CG_DataType_t donor_datatype = CG_DataType_t::CG_DataTypeNull;

                    CG_INFO(cg_conn_info(cg_file_id,
                                         base,
                                         zone,
                                         ncoon,
                                         ncoon_name.data(),
                                         &ncoon_loc,
                                         &connect_type,
                                         &ptset_type,
                                         &npnts,
                                         ncoon_donor_name.data(),
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
                             ncoon_donor_name.c_str(),
                             ndata_donor);
                }
                int nholes = 0;
                CG_INFO(cg_nholes(cg_file_id, base, zone, &nholes));
                for (int hole = 1; hole <= nholes; ++hole) {
                    std::string hole_name(33, '\0');
                    CG_GridLocation_t hole_location = CG_GridLocation_t::CG_GridLocationNull;
                    CG_PointSetType_t hole_ptset_type = CG_PointSetType_t::CG_PointSetTypeNull;
                    int hole_nptsets = 0;
                    cgsize_t hole_npnts = 0;

                    CG_INFO(
                        cg_hole_info(cg_file_id, base, zone, hole, hole_name.data(), &hole_location, &hole_ptset_type, &hole_nptsets, &hole_npnts));

                    LOG_INFO("    [GeneralizedConnectivity]{:>2}:[{}] {}, [{}] nptsets={}, npnts={}",
                             hole,
                             cg_GridLocationName(hole_location),
                             hole_name.c_str(),
                             cg_PointSetTypeName(hole_ptset_type),
                             hole_nptsets,
                             hole_npnts);
                }

                // Boundary Conditions
                int nbocos = 0;
                CG_INFO(cg_nbocos(cg_file_id, base, zone, &nbocos));
                for (int boco = 1; boco <= nbocos; ++boco) {
                    std::string boco_name(33, '\0');
                    CG_BCType_t boco_type = CG_BCType_t::CG_BCTypeNull;
                    CG_PointSetType_t boco_ptset_type = CG_PointSetType_t::CG_PointSetTypeNull;
                    CG_DataType_t boco_normal_datatype = CG_DataType_t::CG_DataTypeNull;
                    CG_GridLocation_t boco_location = CG_GridLocation_t::CG_GridLocationNull;
                    cgsize_t boco_npnts = 0, boco_normal_list_size = 0;
                    int boco_normal_index = 0, boco_ndataset = 0;

                    CG_INFO(cg_boco_gridlocation_read(cg_file_id, base, zone, boco, &boco_location));
                    CG_INFO(cg_boco_info(cg_file_id,
                                         base,
                                         zone,
                                         boco,
                                         boco_name.data(),
                                         &boco_type,
                                         &boco_ptset_type,
                                         &boco_npnts,
                                         &boco_normal_index,
                                         &boco_normal_list_size,
                                         &boco_normal_datatype,
                                         &boco_ndataset));

                    LOG_INFO("    [BoundaryConditions]{:>2}:[{}]-[{}] {}, [{}] npnts={}, [{}] index={}, listSize={}, ndataset={}",
                             boco,
                             cg_BCTypeName(boco_type),
                             cg_GridLocationName(boco_location),
                             boco_name.c_str(),
                             cg_PointSetTypeName(boco_ptset_type),
                             boco_npnts,
                             cg_DataTypeName(boco_normal_datatype),
                             boco_normal_index,
                             boco_normal_list_size,
                             boco_ndataset);

                    for (int boco_dataset = 1; boco_dataset <= boco_ndataset; ++boco_dataset) {
                        std::string boco_dataset_name(33, '\0');
                        CG_BCType_t boco_dataset_type = CG_BCType_t::CG_BCTypeNull;
                        int boco_dataset_dirichlet_flag = 0, boco_dataset_neumann_flag = 0;

                        CG_INFO(cg_dataset_read(cg_file_id,
                                                base,
                                                zone,
                                                boco,
                                                boco_dataset,
                                                boco_dataset_name.data(),
                                                &boco_dataset_type,
                                                &boco_dataset_dirichlet_flag,
                                                &boco_dataset_neumann_flag));

                        LOG_INFO("      [BoundaryConditionsDataset]{:>2}:[{}]-[{}] {}, DirichletFlag={}, NeumannFlag ={}",
                                 boco_dataset,
                                 cg_BCTypeName(boco_dataset_type),
                                 cg_GridLocationName(boco_location),
                                 boco_dataset_name.c_str(),
                                 boco_dataset_dirichlet_flag,
                                 boco_dataset_neumann_flag);
                    }
                }

                // Rigid Grid Motion
                int n_rigid_motions = 0;
                CG_INFO(cg_n_rigid_motions(cg_file_id, base, zone, &n_rigid_motions));
                for (int rigid_motion = 1; rigid_motion <= n_rigid_motions; ++rigid_motion) {
                    std::string rigid_motion_name(33, '\0');
                    CG_RigidGridMotionType_t rigid_motion_type = CG_RigidGridMotionType_t::CG_RigidGridMotionTypeNull;
                    CG_INFO(cg_rigid_motion_read(cg_file_id, base, zone, rigid_motion, rigid_motion_name.data(), &rigid_motion_type));

                    LOG_INFO("    [RigidGridMotion]{:>2}:[{}] {}",
                             rigid_motion,
                             cg_RigidGridMotionTypeName(rigid_motion_type),
                             rigid_motion_name.c_str());
                }

                // Arbitrary Grid Motion
                int n_arbitrary_motions = 0;
                CG_INFO(cg_n_arbitrary_motions(cg_file_id, base, zone, &n_arbitrary_motions));
                for (int arbitrary_motion = 1; arbitrary_motion <= n_arbitrary_motions; ++arbitrary_motion) {
                    std::string arbitrary_motion_name(33, '\0');
                    CG_ArbitraryGridMotionType_t arbitrary_motion_type = CG_ArbitraryGridMotionType_t::CG_ArbitraryGridMotionTypeNull;
                    CG_INFO(cg_arbitrary_motion_read(cg_file_id, base, zone, arbitrary_motion, arbitrary_motion_name.data(), &arbitrary_motion_type));

                    LOG_INFO("    [ArbitraryGridMotion]{:>2}:[{}] {}",
                             arbitrary_motion,
                             cg_ArbitraryGridMotionTypeName(arbitrary_motion_type),
                             arbitrary_motion_name.c_str());
                }

                // Zone Grid Connectivity
                int nzconns = 0;
                CG_INFO(cg_nzconns(cg_file_id, base, zone, &nzconns));
                for (int zconn = 1; zconn <= nzconns; ++zconn) {
                    std::string zconn_name(33, '\0');
                    CG_INFO(cg_zconn_read(cg_file_id, base, zone, zconn, zconn_name.data()));

                    LOG_INFO("    [ZoneGridConnectivity]{:>2}:[NULL] {}", zconn, zconn_name.c_str());
                }
            }

            // Particle Zone Information
            int nparticlezones = 0;
            CG_INFO(cg_nparticle_zones(cg_file_id, base, &nparticlezones));
            for (int particle_zone = 1; particle_zone <= nparticlezones; ++particle_zone) {
                double particle_zone_id = 0;
                std::string particle_zone_name(33, '\0');
                cgsize_t particle_zone_size = 0;

                CG_INFO(cg_particle_id(cg_file_id, base, particle_zone, &particle_zone_id));
                CG_INFO(cg_particle_read(cg_file_id, base, particle_zone, particle_zone_name.data(), &particle_zone_size));

                LOG_INFO("  [Particle]{:>2}:[NULL] {}, id={}, size={}",
                         particle_zone,
                         particle_zone_name.c_str(),
                         particle_zone_id,
                         particle_zone_size);

                // Particle Coordinates
                int particle_ncoord_nodes = 0;
                CG_INFO(cg_particle_ncoord_nodes(cg_file_id, base, particle_zone, &particle_ncoord_nodes));
                for (int particle_zone_coord = 1; particle_zone_coord <= particle_ncoord_nodes; ++particle_zone_coord) {
                    std::string particle_zone_coord_name(33, '\0');
                    CG_DataType_t particle_zone_coord_type = CG_DataType_t::CG_DataTypeNull;

                    CG_INFO(cg_particle_coord_info(cg_file_id,
                                                   base,
                                                   particle_zone,
                                                   particle_zone_coord,
                                                   &particle_zone_coord_type,
                                                   particle_zone_coord_name.data()));

                    LOG_INFO("    [ParticleCoordinates]{:>2}:[{}] {}",
                             particle_zone_coord,
                             cg_DataTypeName(particle_zone_coord_type),
                             particle_zone_coord_name.c_str());
                }

                // Particle Solution
                int particle_nsols = 0;
                CG_INFO(cg_particle_nsols(cg_file_id, base, particle_zone, &particle_nsols));
                for (int particle_sol = 1; particle_sol <= particle_nsols; ++particle_sol) {
                    std::string particle_sol_name(33, '\0');
                    double particle_sol_id = 0.0;
                    cgsize_t particle_sol_size = 0, particle_sol_npnts = 0;
                    std::vector<cgsize_t> particle_sol_pnts;
                    CG_PointSetType_t particle_sol_ptset_type = CG_PointSetType_t::CG_PointSetTypeNull;

                    CG_INFO(cg_particle_sol_info(cg_file_id, base, particle_zone, particle_sol, particle_sol_name.data()));
                    CG_INFO(cg_particle_sol_id(cg_file_id, base, particle_zone, particle_sol, &particle_sol_id));
                    CG_INFO(cg_particle_sol_size(cg_file_id, base, particle_zone, particle_sol, &particle_sol_size));
                    CG_INFO(cg_particle_sol_ptset_info(cg_file_id, base, particle_zone, particle_sol, &particle_sol_ptset_type, &particle_sol_npnts));

                    if (particle_sol_npnts != 0) {
                        particle_sol_pnts.resize(particle_sol_npnts);
                        CG_INFO(cg_particle_sol_ptset_read(cg_file_id, base, particle_zone, particle_sol, particle_sol_pnts.data()));
                    }

                    int particle_nfields = 0;
                    CG_INFO(cg_particle_nfields(cg_file_id, base, particle_zone, particle_sol, &particle_nfields));
                    LOG_INFO("    [ParticleSolution]{:>2}:[{}] {}, id={}, size={}, npnts={}, pnts={}, nfields={}",
                             particle_sol,
                             cg_PointSetTypeName(particle_sol_ptset_type),
                             particle_sol_name.c_str(),
                             particle_sol_id,
                             particle_sol_size,
                             particle_sol_npnts,
                             particle_sol_pnts,
                             particle_nfields);
                }
            }
        }

        return CG_INFO(cg_close(cg_file_id)) == CG_OK;
    }
} // namespace ReaderCGNS
