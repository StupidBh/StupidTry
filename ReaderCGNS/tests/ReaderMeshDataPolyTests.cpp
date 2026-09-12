#include "FileManager.h"
#include "CgnsTopology.hpp"

#include "ReaderMeshData.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
    class TestReaderMeshData final : public ReaderMeshData {
    public:
        bool GetAllFieldFunctionName(std::vector<std::string>&) final { return false; }

        bool GetFieldFunctionData(const std::string&, ReaderAPI::Field&) final { return false; }

        bool GetFieldFunctionData(const std::vector<std::string>&, std::vector<ReaderAPI::Field>&) final { return false; }

        void info() const final { }

        std::string GetSolverType() const final { return { }; }
    };

    void expect(const bool condition, const char* message)
    {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    SectionTopology make_ngon()
    {
        SectionTopology section;
        section.type = CG_ElementType_t::CG_NGON_n;
        section.name = "Base.Zone.Faces";
        section.range_start = 10;
        section.range_end = 11;
        section.elements = { 1, 2, 3, 1, 3, 4 };
        section.connect_offset = { 0, 3, 6 };
        return section;
    }

    SectionTopology make_nface()
    {
        SectionTopology section;
        section.type = CG_ElementType_t::CG_NFACE_n;
        section.name = "Base.Zone.Cells";
        section.range_start = 100;
        section.range_end = 101;
        section.elements = { 10, -11, 999 };
        section.connect_offset = { 0, 2, 3 };
        return section;
    }

    void test_raw_face_ids_and_reversed_references()
    {
        TestReaderMeshData data;
        data.m_ngon_nface = { make_nface(), make_ngon() };
        data.fatten_section_elem_poly(false);

        expect(data.m_elements.size() == 1, "invalid NFACE references must not discard valid faces");
        const auto& element = data.m_elements.front();
        expect(element.id == 100, "NFACE element ID must retain its original range offset");
        expect(element.npts == 2, "NFACE npts must count retained faces");
        expect(element.nodes == std::vector<ReaderAPI::Integer> { 3, 0, 1, 2, 3, 3, 2, 0 }, "negative face references must reverse a private face copy");
        expect(data.m_ngon_nface.empty(), "poly sections must be consumed after expansion");
    }

    void test_separate_surface_output()
    {
        TestReaderMeshData data;
        data.m_ngon_nface = { make_ngon() };
        data.fatten_section_elem_poly(true);

        expect(data.m_elements.size() == 2, "separate_surface must emit every valid NGON face");
        expect(data.m_elements[0].id == 0 && data.m_elements[1].id == 1, "surface IDs must use the current element offset");
        expect(data.m_elements[0].type == static_cast<ReaderAPI::Integer>(CG_ElementType_t::CG_NGON_n), "surface elements must retain the NGON type");
        expect(data.m_components.size() == 1 && data.m_components.contains("Base.Zone.Faces"), "surface output must register its element set");
    }
} // namespace

int main()
{
    test_raw_face_ids_and_reversed_references();
    test_separate_surface_output();
    return EXIT_SUCCESS;
}
