#include "CgnsCore.h"

#include <set>

#include "cgnslib.h"
#include "Logger/logger.hpp"

int CgnsCore::CG_INFO(int status, const std::filesystem::path& file, int line)
{
    switch (status) {
    case CG_OK   : return CG_OK;
    case CG_ERROR: {
        LOG_ERROR("[{}:{}] [CG_ERROR]: {}", file.filename(), line, cg_get_error());
        return CG_ERROR;
    }
    case CG_NODE_NOT_FOUND: {
        LOG_WARN("[{}:{}] [CG_NODE_NOT_FOUND]: {}", file.filename(), line, cg_get_error());
        return CG_NODE_NOT_FOUND;
    }
    case CG_INCORRECT_PATH: {
        LOG_WARN("[{}:{}] [CG_INCORRECT_PATH]: {}", file.filename(), line, cg_get_error());
        return CG_INCORRECT_PATH;
    }
    case CG_NO_INDEX_DIM: {
        LOG_WARN("[{}:{}] [CG_NO_INDEX_DIM]: {}", file.filename(), line, cg_get_error());
        return CG_NO_INDEX_DIM;
    }

    default: {
        LOG_WARN("Unknown status.");
        return status;
    }
    }
}

#define CG_INFO(STATUS) CG_INFO(STATUS, __FILE__, __LINE__)

CgnsCore::CgnsCore(const std::string& cgns_file_path) :
    m_cgns_file_path(cgns_file_path)
{
}

CgnsCore::~CgnsCore()
{
    this->CloseCGNS();
}

bool CgnsCore::OpenCGNS()
{
    LOG_INFO("Open in read only: [{}]", this->m_cgns_file_path);

    int cgns_file_type = -1;
    const int status = cg_is_cgns(this->m_cgns_file_path.c_str(), &cgns_file_type);
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
        this->m_cg_file_id = 0;
        return false;
    }

    if (cg_open(this->m_cgns_file_path.c_str(), CG_MODE_READ, &this->m_cg_file_id) != CG_OK) {
        LOG_ERROR("Open [{}] failed: {}", this->m_cgns_file_path, cg_get_error());
        this->m_cg_file_id = 0;
        return false;
    }

    float cg_file_version = 0.F;
    int cg_file_precision = 0;
    CG_INFO(cg_version(this->m_cg_file_id, &cg_file_version));
    CG_INFO(cg_precision(this->m_cg_file_id, &cg_file_precision));
    LOG_INFO("{}:[{}] v{:.2f}, precision={}",
             this->m_cg_file_id,
             FileTypeName(cgns_file_type),
             cg_file_version,
             cg_file_precision);
    return true;
}

bool CgnsCore::OpenCGNS(const std::string& cgns_file_path)
{
    this->CloseCGNS();
    this->m_cgns_file_path = cgns_file_path;
    return this->OpenCGNS();
}

void CgnsCore::CloseCGNS()
{
    if (this->m_cg_file_id != 0) {
        CG_INFO(cg_close(this->m_cg_file_id));
        this->m_cg_file_id = 0;
    }
    this->m_cgns_file_path = "";
}

void CgnsCore::info() const
{
    if (!this->IsOpen()) {
        LOG_WARN("info() called on a file that is not open; skipping.");
        return;
    }

    int nbases = 0;
    CG_INFO(cg_nbases(this->m_cg_file_id, &nbases));
    for (int base = 1; base <= nbases; ++base) {
        std::string base_name(33, '\0');
        int cell_dim = 0, phys_dim = 0;
        CG_SimulationType_t base_simulation_type = CG_SimulationType_t::CG_SimulationTypeNull;
        CG_INFO(cg_base_read(this->m_cg_file_id, base, base_name.data(), &cell_dim, &phys_dim));
        CG_INFO(cg_simulation_type_read(this->m_cg_file_id, base, &base_simulation_type));
        LOG_INFO("[Base]{:>2}:[{}] {}, CellDimension={}, PhysicalDimension={}",
                 base,
                 cg_SimulationTypeName(base_simulation_type),
                 base_name,
                 cell_dim,
                 phys_dim);

        int nzones = 0;
        CG_INFO(cg_nzones(this->m_cg_file_id, base, &nzones));
        for (int zone = 1; zone <= nzones; ++zone) {
            std::string zone_name(33, '\0');
            int index_zone_dim = -1;
            CG_ZoneType_t zone_type = CG_ZoneType_t::CG_ZoneTypeNull;
            std::array<cgsize_t, 9> zone_size { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

            CG_INFO(cg_zone_type(this->m_cg_file_id, base, zone, &zone_type));
            CG_INFO(cg_index_dim(this->m_cg_file_id, base, zone, &index_zone_dim));
            CG_INFO(cg_zone_read(this->m_cg_file_id, base, zone, zone_name.data(), zone_size.data()));

            cgsize_t vertex_sum = 0, cell_sum = 0;
            if (zone_type == CG_ZoneType_t::CG_Structured) {
                switch (index_zone_dim) {
                case 1: {
                    LOG_INFO("  [Zone]{:>3}:[{}] {}, Dim={}",
                             zone,
                             cg_ZoneTypeName(zone_type),
                             zone_name,
                             index_zone_dim);
                } break;
                case 2: {
                    vertex_sum = zone_size[0] * zone_size[1];
                    cell_sum = zone_size[2] * zone_size[3];
                    LOG_INFO(
                        "  [Zone]{:>3}:[{}] {}, Dim={}, NVertex=[{},{}]:{}, NCell=[{},{}]:{}, NBoundVertex=[{},{}]",
                        zone,
                        cg_ZoneTypeName(zone_type),
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
                    vertex_sum = zone_size[0] * zone_size[1] * zone_size[2];
                    cell_sum = zone_size[3] * zone_size[4] * zone_size[5];
                    LOG_INFO(
                        "  [Zone]{:>3}:[{}] {}, Dim={}, NVertex=[{},{},{}]:{}, NCell=[{},{},{}]:{}, "
                        "NBoundVertex=[{},{},{}]",
                        zone,
                        cg_ZoneTypeName(zone_type),
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
                    LOG_WARN("  [Zone]{:>3}:[{}] {}, Invalid-Dim={}",
                             zone,
                             cg_ZoneTypeName(zone_type),
                             zone_name,
                             index_zone_dim);
                } break;
                }
            }
            else if (zone_type == CG_ZoneType_t::CG_Unstructured) {
                vertex_sum = zone_size[0];
                cell_sum = zone_size[1];

                LOG_INFO("  [Zone]{:>3}:[{}] {}, NVertex={}, NCell={}, NBoundVertex={}",
                         zone,
                         cg_ZoneTypeName(zone_type),
                         zone_name,
                         zone_size[0],
                         zone_size[1],
                         zone_size[2]);
            }

            // Flow Solution
            int nsols = 0;
            CG_INFO(cg_nsols(this->m_cg_file_id, base, zone, &nsols));
            for (int sol = 1; sol <= nsols; ++sol) {
                std::string sol_name(33, '\0');
                int sol_data_dim = 0, nfields = 0;
                std::vector<cgsize_t> sol_npnts(1, 0);
                std::vector<cgsize_t> sol_dim_vals(index_zone_dim, 0);
                CG_GridLocation_t sol_location = CG_GridLocation_t::CG_GridLocationNull;
                CG_PointSetType_t sol_point_set_type = CG_PointSetType_t::CG_PointSetTypeNull;
                CG_INFO(cg_nfields(this->m_cg_file_id, base, zone, sol, &nfields));
                CG_INFO(cg_sol_info(this->m_cg_file_id, base, zone, sol, sol_name.data(), &sol_location));
                CG_INFO(cg_sol_size(this->m_cg_file_id, base, zone, sol, &sol_data_dim, sol_dim_vals.data()));
                CG_INFO(cg_sol_ptset_info(this->m_cg_file_id, base, zone, sol, &sol_point_set_type, sol_npnts.data()));

                if (sol_point_set_type != CG_PointSetType_t::CG_PointSetTypeNull) {
                    sol_npnts.resize(sol_npnts[0], 0);
                    CG_INFO(cg_sol_ptset_read(this->m_cg_file_id, base, zone, sol, sol_npnts.data()));
                }

                LOG_INFO("    [FlowSolution]{:>2}:[{}]-[{}] {}, NField={}, DataDim={}, DataVal={}, npnts={}",
                         sol,
                         cg_GridLocationName(sol_location),
                         cg_PointSetTypeName(sol_point_set_type),
                         sol_name,
                         nfields,
                         sol_data_dim,
                         sol_dim_vals,
                         sol_npnts);
            }

            // Discrete Data
            int ndiscrete = 0;
            CG_INFO(cg_ndiscrete(this->m_cg_file_id, base, zone, &ndiscrete));
            for (int discrete = 1; discrete <= ndiscrete; ++discrete) {
                std::string discrete_name(33, '\0');
                int discrete_data_dim = 0;
                std::vector<cgsize_t> discrete_dim_vals(index_zone_dim, 0), discrete_npnts(1, 0);
                CG_PointSetType_t discrete_point_set_type = CG_PointSetType_t::CG_PointSetTypeNull;

                CG_INFO(cg_discrete_read(this->m_cg_file_id, base, zone, discrete, discrete_name.data()));
                CG_INFO(cg_discrete_size(this->m_cg_file_id,
                                         base,
                                         zone,
                                         discrete,
                                         &discrete_data_dim,
                                         discrete_dim_vals.data()));
                CG_INFO(cg_discrete_ptset_info(this->m_cg_file_id,
                                               base,
                                               zone,
                                               discrete,
                                               &discrete_point_set_type,
                                               discrete_npnts.data()));

                if (discrete_point_set_type == CG_PointSetType_t::CG_PointSetTypeNull) {
                    discrete_npnts.resize(discrete_npnts[0], 0);
                    CG_INFO(cg_discrete_ptset_read(this->m_cg_file_id, base, zone, discrete, discrete_npnts.data()));
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
            CG_INFO(cg_nsubregs(this->m_cg_file_id, base, zone, &nsubregs));
            for (int subreg = 1; subreg <= nsubregs; ++subreg) {
                std::string subreg_name(33, '\0');
                int subreg_dim = 0, subreg_bcname_len = 0, subreg_gcname_len;
                std::vector<cgsize_t> subreg_npnts(1, 0);
                CG_GridLocation_t subreg_location = CG_GridLocation_t::CG_GridLocationNull;
                CG_PointSetType_t subreg_point_set_type = CG_PointSetType_t::CG_PointSetTypeNull;

                CG_INFO(cg_subreg_info(this->m_cg_file_id,
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
                    subreg_npnts.resize(subreg_npnts[0], 1);
                    CG_INFO(cg_subreg_ptset_read(this->m_cg_file_id, base, zone, subreg, subreg_npnts.data()));
                }

                std::string msg = std::format("    [ZoneSubregions]{:>2}:[{}]-[{}] {}, Dimension={}",
                                              subreg,
                                              cg_GridLocationName(subreg_location),
                                              cg_PointSetTypeName(subreg_point_set_type),
                                              subreg_name,
                                              subreg_dim);
                if (subreg_bcname_len > 0) {
                    std::string subreg_bcname(subreg_bcname_len, '\0');
                    CG_INFO(cg_subreg_bcname_read(this->m_cg_file_id, base, zone, subreg, subreg_bcname.data()));
                    msg += std::format(" baname={}", subreg_bcname);
                }
                if (subreg_gcname_len > 0) {
                    std::string subreg_gcname(subreg_gcname_len, '\0');
                    CG_INFO(cg_subreg_gcname_read(this->m_cg_file_id, base, zone, subreg, subreg_gcname.data()));
                    msg += std::format(" gcname={}", subreg_gcname);
                }
                LOG_INFO("{} npnts={}", msg, subreg_npnts);
            }

            // Zone Grid Coordinates
            int ngrids = 0;
            CG_INFO(cg_ngrids(this->m_cg_file_id, base, zone, &ngrids));
            for (int grid = 1; grid <= ngrids; ++grid) {
                std::string grid_name(33, '\0');
                CG_DataType_t grid_data_type = CG_DataType_t::CG_DataTypeNull;
                std::vector<cgsize_t> grid_boundingbox(1, 0);

                CG_INFO(cg_grid_read(this->m_cg_file_id, base, zone, grid, grid_name.data()));
                CG_INFO(cg_grid_bounding_box_read(this->m_cg_file_id,
                                                  base,
                                                  zone,
                                                  grid,
                                                  grid_data_type,
                                                  grid_boundingbox.data()));

                LOG_INFO("    [ZoneGird]{:>2}:[{}] {}", grid, cg_DataTypeName(grid_data_type), grid_name);
            }
            int ncoords = 0;
            CG_INFO(cg_ncoords(this->m_cg_file_id, base, zone, &ncoords));
            for (int coord = 1; coord <= ncoords; ++coord) {
                std::string coord_name(33, '\0');
                CG_DataType_t coord_data_type = CG_DataType_t::CG_DataTypeNull;
                CG_INFO(cg_coord_info(this->m_cg_file_id, base, zone, coord, &coord_data_type, coord_name.data()));

                LOG_INFO("    [ZoneCoords]{:>2}:[{}] {}", coord, cg_DataTypeName(coord_data_type), coord_name);
            }

            // Element Connectivity
            int nsections = 0;
            CG_INFO(cg_nsections(this->m_cg_file_id, base, zone, &nsections));
            for (int section = 1; section <= nsections; ++section) {
                std::string section_name(33, '\0');
                CG_ElementType_t section_element_type = CG_ElementType_t::CG_ElementTypeNull;
                cgsize_t section_start = 0, section_end = 0;
                int section_nbndry = 0, section_parent_flag = 0;
                CG_INFO(cg_section_read(this->m_cg_file_id,
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
                CG_INFO(cg_ElementDataSize(this->m_cg_file_id, base, zone, section, &element_data_size));

                std::vector<cgsize_t> elements(element_data_size, 0);
                cgsize_t section_element_sum = section_end - section_start + 1;
                static const std::set MIX_ELEMENT = { CG_ElementType_t::CG_MIXED,
                                                      CG_ElementType_t::CG_NGON_n,
                                                      CG_ElementType_t::CG_NFACE_n };
                if (MIX_ELEMENT.contains(section_element_type)) {
                    std::vector<cgsize_t> elements_connect_offset(section_element_sum + 1, 0);
                    CG_INFO(cg_poly_elements_read(this->m_cg_file_id,
                                                  base,
                                                  zone,
                                                  section,
                                                  elements.data(),
                                                  elements_connect_offset.data(),
                                                  nullptr));
                }
                else {
                    CG_INFO(cg_elements_read(this->m_cg_file_id, base, zone, section, elements.data(), nullptr));
                }

                if (elements.empty()) {
                    LOG_INFO("    [ElementConnectivity]{:>3}:[{}] {}, ElementRange=<empty>:{}",
                             section,
                             cg_ElementTypeName(section_element_type),
                             section_name,
                             section_element_sum);
                }
                else {
                    auto by_abs = [](auto value) {
                        return std::abs(value);
                    };
                    LOG_INFO("    [ElementConnectivity]{:>3}:[{}] {}, ElementRange=[{},{}]:{}",
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
            CG_INFO(cg_n1to1(this->m_cg_file_id, base, zone, &n1to1s));
            for (int n1to1 = 1; n1to1 <= n1to1s; ++n1to1) {
                std::string n1to1_connectname(33, '\0');
                std::string n1to1_donorname(33, '\0');
                cgsize_t n1to1_range = 0, donor_range = 0;
                int transform = 0;

                CG_INFO(cg_1to1_read(this->m_cg_file_id,
                                     base,
                                     zone,
                                     n1to1,
                                     n1to1_connectname.data(),
                                     n1to1_donorname.data(),
                                     &n1to1_range,
                                     &donor_range,
                                     &transform));
            }
        }
    }
}

bool CgnsCore::IsOpen() const
{
    return this->m_cg_file_id != 0;
}
