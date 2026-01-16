#include "CgnsCore.h"
#include "CgnsUtils.h"

void cgns::InitLog(std::shared_ptr<spdlog::logger> log)
{
    dylog::Logger::get_instance().UpdateLog(log);
}

void cgns::OpenCGNS(const std::string& file_path)
{
    LOG_INFO("Open in read only: [{}]", file_path);

    int cgns_file_type = -1;
    int status = cg_is_cgns(file_path.c_str(), &cgns_file_type);
    auto FileTypeName = [](int file_type) -> const char* {
        switch (file_type) {
        case CG_FILE_ADF : return "ADF";
        case CG_FILE_ADF2: return "ADF2";
        case CG_FILE_HDF5: return "HDF5";
        default          : return "ERROR_FILE";
        }
    };
    if (status != CG_OK || cgns_file_type == CG_FILE_NONE) {
        LOG_INFO("The file is a invalid [{}] file, msg: {}", FileTypeName(cgns_file_type), cg_get_error());
        return;
    }

    int cg_file_id = 0;
    if (CG_INFO(cg_open(file_path.c_str(), CG_MODE_READ, &cg_file_id)) != CG_OK) {
        LOG_ERROR("Open [{}] failed: {}", file_path, cg_file_id);
        return;
    }

    float cg_file_version = 0.F;
    int cg_file_precision = 0;
    CG_INFO(cg_version(cg_file_id, &cg_file_version));
    CG_INFO(cg_precision(cg_file_id, &cg_file_precision));
    LOG_INFO(
        "{}:[{}] v{:.2f}, precision={}",
        cg_file_id,
        FileTypeName(cgns_file_type),
        cg_file_version,
        cg_file_precision);

    int nbases = 0;
    CG_INFO(cg_nbases(cg_file_id, &nbases));
    for (int base = 1; base <= nbases; ++base) {
        char base_name[33];
        int cell_dim = 0, phys_dim = 0;
        SimulationType_t base_simulation_tyoe = SimulationType_t::SimulationTypeNull;
        CG_INFO(cg_base_read(cg_file_id, base, base_name, &cell_dim, &phys_dim));
        CG_INFO(cg_simulation_type_read(cg_file_id, base, &base_simulation_tyoe));
        LOG_INFO(
            "{:>2}:[{}] {}, CellDimension={}, PhysicalDimension={}",
            base,
            SimulationTypeName[base_simulation_tyoe],
            base_name,
            cell_dim,
            phys_dim);

        int nzones = 0;
        CG_INFO(cg_nzones(cg_file_id, base, &nzones));
        for (int zone = 1; zone <= nzones; ++zone) {
            char zone_name[33];
            ZoneType_t zone_type = ZoneType_t::ZoneTypeNull;
            std::array<cgsize_t, 9> zone_size { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
            CG_INFO(cg_zone_type(cg_file_id, base, zone, &zone_type));
            CG_INFO(cg_zone_read(cg_file_id, base, zone, zone_name, zone_size.data()));
            if (zone_type == ZoneType_t::Structured) {
                int index_zone_dim = -1;
                CG_INFO(cg_index_dim(cg_file_id, base, zone, &index_zone_dim));
                switch (index_zone_dim) {
                case 1: {
                    LOG_INFO("  {:>3}:[{}] {}, Dim={}", zone, ZoneTypeName[zone_type], zone_name, index_zone_dim);
                } break;
                case 2: {
                    cgsize_t vertex_sum = zone_size[0] * zone_size[1];
                    cgsize_t cell_sum = zone_size[2] * zone_size[3];
                    LOG_INFO(
                        "  {:>3}:[{}] {}, Dim={}, NVertex=[{},{}]:{}, NCell=[{},{}]:{}, NBoundVertex=[{},{}]",
                        zone,
                        ZoneTypeName[zone_type],
                        zone_name,
                        index_zone_dim,
                        zone_size[0],
                        zone_size[1],
                        vertex_sum,
                        zone_size[2],
                        zone_size[3],
                        cell_sum,
                        zone_size[4],
                        zone_size[5]);

                } break;
                case 3: {
                    cgsize_t vertex_sum = zone_size[0] * zone_size[1] * zone_size[2];
                    cgsize_t cell_sum = zone_size[3] * zone_size[4] * zone_size[5];
                    LOG_INFO(
                        "  {:>3}:[{}] {}, Dim={}, NVertex=[{},{},{}]:{}, NCell=[{},{},{}]:{}, NBoundVertex=[{},{},{}]",
                        zone,
                        ZoneTypeName[zone_type],
                        zone_name,
                        index_zone_dim,
                        zone_size[0],
                        zone_size[1],
                        zone_size[2],
                        vertex_sum,
                        zone_size[3],
                        zone_size[4],
                        zone_size[5],
                        cell_sum,
                        zone_size[6],
                        zone_size[7],
                        zone_size[8]);
                } break;
                default: {
                    LOG_WARN(
                        "  {:>3}:[{}] {}, Invaild-Dim={}",
                        zone,
                        ZoneTypeName[zone_type],
                        zone_name,
                        index_zone_dim);
                } break;
                }
            }
            else if (zone_type == ZoneType_t::Unstructured) {
                LOG_INFO(
                    "  {:>3}:[{}] {}, NVertex={}, NCell={}, NBoundVertex={}",
                    zone,
                    ZoneTypeName[zone_type],
                    zone_name,
                    zone_size[0],
                    zone_size[1],
                    zone_size[2]);
            }

            int nsols = 0;
            CG_INFO(cg_nsols(cg_file_id, base, zone, &nsols));
            for (int sol = 1; sol <= nsols; ++sol) {
                char sol_name[33];
                int sol_data_dim = 0, nfields = 0;
                cgsize_t sol_npnts = 0;
                std::vector<cgsize_t> sol_dim_vals(4, 0);
                GridLocation_t sol_location = GridLocation_t::GridLocationNull;
                PointSetType_t sol_point_set_type = PointSetType_t::PointSetTypeNull;
                CG_INFO(cg_nfields(cg_file_id, base, zone, sol, &nfields));
                CG_INFO(cg_sol_info(cg_file_id, base, zone, sol, sol_name, &sol_location));
                CG_INFO(cg_sol_size(cg_file_id, base, zone, sol, &sol_data_dim, sol_dim_vals.data()));
                CG_INFO(cg_sol_ptset_info(cg_file_id, base, zone, sol, &sol_point_set_type, &sol_npnts));

                sol_dim_vals.resize(sol_data_dim);
                LOG_INFO(
                    "    {:>2}:[{}]-[{}] {}, NField={}, DataDim={}, DataVal={}, npnts={}",
                    sol,
                    GridLocationName[sol_location],
                    PointSetTypeName[sol_point_set_type],
                    sol_name,
                    nfields,
                    sol_data_dim,
                    sol_dim_vals,
                    sol_npnts);
            }
            LOG->flush();

            int nsections = 0;
            CG_INFO(cg_nsections(cg_file_id, base, zone, &nsections));
            for (int section = 1; section <= nsections; ++section) {
                char section_name[33];
                ElementType_t section_element_type = ElementType_t::ElementTypeNull;
                cgsize_t section_start = 0, section_end = 0;
                int section_nbndry = 0, section_parent_flag = 0;
                CG_INFO(cg_section_read(
                    cg_file_id,
                    base,
                    zone,
                    section,
                    section_name,
                    &section_element_type,
                    &section_start,
                    &section_end,
                    &section_nbndry,
                    &section_parent_flag));

                cgsize_t section_element_sum = section_end - section_start + 1;
                cgsize_t element_data_size = 0;
                CG_INFO(cg_ElementDataSize(cg_file_id, base, zone, section, &element_data_size));

                std::vector<cgsize_t> elements(element_data_size, 0);
                static constexpr std::array MIX_ELEMENT = { ElementType_t::MIXED,
                                                            ElementType_t::NGON_n,
                                                            ElementType_t::NFACE_n };
                if (std::ranges::find(MIX_ELEMENT, section_element_type) != MIX_ELEMENT.end()) {
                    std::vector<cgsize_t> elements_connect_offset(section_element_sum + 1, 0);
                    CG_INFO(cg_poly_elements_read(
                        cg_file_id,
                        base,
                        zone,
                        section,
                        elements.data(),
                        elements_connect_offset.data(),
                        nullptr));
                }
                else {
                    CG_INFO(cg_elements_read(cg_file_id, base, zone, section, elements.data(), nullptr));
                }

                LOG_INFO(
                    "   {:>3}:[{}] {}, ElementRange=[{},{}]:{}",
                    section,
                    ElementTypeName[section_element_type],
                    section_name,
                    std::ranges::min(elements, std::ranges::less {}, [](auto value) { return std::abs(value); }),
                    std::ranges::max(elements, std::ranges::less {}, [](auto value) { return std::abs(value); }),
                    section_element_sum);
                LOG->flush();
            }
        }
    }

    CG_INFO(cg_close(cg_file_id));
}
