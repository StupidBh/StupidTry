#include "Function.h"
#include "SingletonData.h"

#include "CgnsCore.h"
#include "HDF5.hpp"
#include "HighFive.hpp"

int main(int argc, char* argv[])
{
    SCOPED_TIMER("stupid-try main");
    SINGLE_DATA.ProcessArguments(argc, argv);
    if (SINGLE_DATA_VM.empty()) {
        return EXIT_FAILURE;
    }

    cgns::InitLog(LOG);
    cgns::OpenCGNS(INPUT_PATH);

    const std::string HF_FILE = "./Try1.h5";
    const std::string H5_FILE = "./Try2.h5";

    {
        std::vector<float> output_data(1'000'000, 2.F);

        SCOPED_TIMER("WriteDataSet");
        HighFive::File file(HF_FILE, HighFive::File::Overwrite);
        WriteDataSet("Try", output_data, file);
    }

    {
        std::vector<float> output_data(1'000'000);

        SCOPED_TIMER("ReadDataSet");
        HighFive::File file(HF_FILE, HighFive::File::ReadOnly);
        auto dataset = file.getDataSet("Try");
        dataset.read(output_data);
    }

    {
        using TestData = HDF5Utils::CompTypeMatrix<std::uint32_t, 8>;

        std::vector<std::string> columns = { "X", "Y", "Z" };
        SCOPED_TIMER("WriteDataSet");
        H5::H5File file(H5_FILE.c_str(), H5F_ACC_TRUNC);

        hsize_t dims = 1'000'000;
        H5::DataSpace dataspace(1, &dims);
        std::vector<TestData> try_data(dims);

        auto comp_type = TestData::CompType(columns);
        H5::DataSet dataset = file.createDataSet("Try", comp_type, dataspace);
        dataset.write(try_data.data(), comp_type);

        file.close();
    }

    {
        using TestData = HDF5Utils::CompTypeMatrix<std::uint32_t, 8>;

        SCOPED_TIMER("ReadDataSet");
        H5::H5File file(H5_FILE.c_str(), H5F_ACC_RDONLY);
        H5::DataSet dataset = file.openDataSet("Try");

        std::vector<std::string> columnNames = HDF5Utils::GetDataSetMemberName(dataset);
        if (columnNames.size() > 8) {
            LOG_ERROR("Too many columns in the dataset!");
            return EXIT_FAILURE;
        }
        auto comp_type = TestData::CompType(columnNames);

        hsize_t dims = 0;
        dataset.getSpace().getSimpleExtentDims(&dims);
        std::vector<TestData> try_data(dims);
        dataset.read(try_data.data(), comp_type);

        file.close();
    }

    spdlog::shutdown();
    return 0;
}
