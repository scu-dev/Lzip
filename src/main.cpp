#include <string>
#include <iostream>
#include <CLI/cli.hpp>
#include <CLI/App.hpp>

#include "compress.hpp"
#include "decompress.hpp"
#include "error.hpp"
#include "meta.hpp"
#include "platform.hpp"

int main(int argc, char** argv) {
    using std::string, std::cout, std::cerr, std::endl, CLI::App, CLI::CallForAllHelp, CLI::CallForHelp, CLI::CallForVersion, CLI::ParseError;
    #if _LZIP_WINDOWS
        SetConsoleCP(CP_UTF8);
        SetConsoleOutputCP(CP_UTF8);
    #endif

    App app;
    app.name(LZIP_APP_NAME);
    app.allow_windows_style_options(false);
    app.allow_config_extras(false);
    app.set_help_all_flag("");
    app.set_help_flag("-h,--help", "输出这条帮助信息并退出。");
    app.set_version_flag("-v, --version", LZIP_SEMATIC_VERSION, "显示版本信息并退出。");
    app.set_config("");
    app.footer(LZIP_COPYRIGHT_NOTICE);
    {
        string inputFile, outputFile;
        auto* add = app.add_subcommand("c", "压缩文件操作");
        add->add_option("input", inputFile, "需要被压缩的文件")->required();
        add->add_option("output", outputFile, "输出文件（可选）");
        add->callback([&inputFile, &outputFile]() {
            if (!Lzip::compressFile(inputFile, outputFile)) {
                cerr << Lzip::Util::getLastError() << endl;
                exit(1);
            }
        });
    }
    {
        string inputFile, outputFile;
        auto* add = app.add_subcommand("d", "解压文件操作");
        add->add_option("input", inputFile, "需要被解压的文件")->required();
        add->add_option("output", outputFile, "输出文件（可选）");
        add->callback([&inputFile, &outputFile]() {
            if (!Lzip::decompressFile(inputFile, outputFile)) {
                cerr << Lzip::Util::getLastError() << endl;
                exit(1);
            }
        });
    }
    try { app.parse(argc, argv); }
    catch (const CallForHelp& e) {
        cout << app.help("", CLI::AppFormatMode::All) << endl;
        exit(0);
    }
    catch (const CallForVersion& e) {
        cout << LZIP_SEMATIC_VERSION << endl;
        exit(0);
    }
    catch (const ParseError& e) {
        cerr << "参数错误：(" << e.get_exit_code() << ")" << e.get_name() << " " << e.what() << endl;
        exit(1);
    }
    return 0;
}