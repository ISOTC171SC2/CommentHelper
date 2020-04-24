/****************************************************************
 *
 * Author: Leonard Rosenthol (lrosenth@adobe.com)
 *
 *
 ****************************************************************/
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "boost/program_options.hpp"
#include "boost/format.hpp"
#include "boost/filesystem.hpp"
#include <boost/process.hpp>
#include <boost/process/exception.hpp>

// LOTS of stuff for logging
#include "boost/log/trivial.hpp"
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/logger.hpp>
#ifdef LOG_TO_FILE
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#endif
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/attributes/named_scope.hpp>


#include "CommentHelperVers.h"

namespace  { 
    const size_t ERROR_IN_COMMAND_LINE = 1;
    const size_t SUCCESS = 0;
    const size_t ERROR_UNHANDLED_EXCEPTION = 2;
 
} // namespace 

static void InitBoostLog(void)
{
    /* init boost log
     * 1. Add common attributes
     * 2. set log filter to trace
     */
    boost::log::add_common_attributes();
    boost::log::core::get()->add_global_attribute("Scope",
                                                  boost::log::attributes::named_scope());
#ifdef _DEBUG
    boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::trace );
#else
    // no debugging or tracing in debug mode!
    boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::info );
#endif
    
    /* log formatter:
     *  Full == [TimeStamp] [ThreadId] [Severity Level] [Scope] Log message
        Simple == [TimeStamp] [Severity Level] Log message
     */
    auto fmtTimeStamp = boost::log::expressions::
    // format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f");
    format_date_time<boost::posix_time::ptime>("TimeStamp", "%H:%M:%S.%f");
    auto fmtThreadId = boost::log::expressions::
    attr<boost::log::attributes::current_thread_id::value_type>("ThreadID");
    auto fmtSeverity = boost::log::expressions::
    attr<boost::log::trivial::severity_level>("Severity");
    auto fmtScope = boost::log::expressions::format_named_scope("Scope",
                                                                boost::log::keywords::format = "%n(%f:%l)",
                                                                boost::log::keywords::iteration = boost::log::expressions::reverse,
                                                                boost::log::keywords::depth = 2);
#ifdef USE_FULL
    boost::log::formatter logFmt =
        boost::log::expressions::format("[%1%] (%2%) [%3%] [%4%] %5%")
            % fmtTimeStamp % fmtThreadId % fmtSeverity % fmtScope
            % boost::log::expressions::smessage;
#else
    boost::log::formatter logFmt = boost::log::expressions::format("[%1%] [%3%] %5%")
                % fmtTimeStamp % fmtThreadId % fmtSeverity % fmtScope
                % boost::log::expressions::smessage;
#endif
    
    /* console sink */
    auto consoleSink = boost::log::add_console_log(std::clog);
    consoleSink->set_formatter(logFmt);
    
#ifdef LOG_TO_FILE
    /* fs sink */
    auto fsSink = boost::log::add_file_log(
                                           boost::log::keywords::file_name = "test_%Y-%m-%d_%H-%M-%S.%N.log",
                                           boost::log::keywords::rotation_size = 10 * 1024 * 1024,
                                           boost::log::keywords::min_free_space = 30 * 1024 * 1024,
                                           boost::log::keywords::open_mode = std::ios_base::app);
    fsSink->set_formatter(logFmt);
    fsSink->locked_backend()->auto_flush(true);
#endif
}

static std::string GetAppInfo()
{
    boost::format cgv("CommentHelper v%1%.%2%.%3%.%4% (%5%)");
    cgv % CommentHelper_VERSION_MAJOR % CommentHelper_VERSION_MINOR
        % CommentHelper_VERSION_PATCH % CommentHelper_VERSION_TWEAK
        % CommentHelper_VERSION_GIT;
    return cgv.str();
}

static std::string _ReadFile(const std::string &fileName)
{
    std::ifstream ifs(fileName.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
    
    std::ifstream::pos_type fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    
    std::vector<char> bytes(fileSize);
    ifs.read(bytes.data(), fileSize);
    
    return std::string(bytes.data(), fileSize);
}

static void _WriteToFile(const std::string& filePath, const std::string& inData)
{
    std::ofstream ofs(filePath.c_str(), std::ios::out);
    ofs << inData;
}

std::string _GetIssues(const std::string& workingDir=".")
{
    std::ostringstream outStr;

    auto binary_path = boost::process::search_path("hub");
    if (binary_path.empty()) {
        BOOST_LOG_TRIVIAL(error) << "Unable to locate `hub`" << std::endl;
    } else {
        try {
            boost::process::ipstream is; //reading pipe-stream
            boost::process::child c(binary_path,
                                    boost::process::args({"issue", "-f", "%b%n-*-*-*-%n"}),
                                    boost::process::start_dir(workingDir),
                                    boost::process::std_out > is);

            std::string line;
            while (c.running() && std::getline(is, line)) {
                outStr << line << std::endl;
            }

            c.wait(); //wait for the process to exit
        }
        catch( boost::process::process_error e) {
            BOOST_LOG_TRIVIAL(error) << "Exception (" << e.code() << ") " << "running 'hub': " << e.what() << std::endl;
        }
        catch (std::exception& e)
        {
            BOOST_LOG_TRIVIAL(error) << "Exception running 'hub': " << e.what() << std::endl;
        }
    }
    
    return outStr.str();
}

std::string _ComputeOutputPath(const std::string& inFile = "", 
                                const std::string& inOutPath="", 
                                const std::string& inOutExt="" )
{
    std::string outPath;

    // setup the output path
    //  either in the same dir as the original or the output location
    if ( inOutPath.length() ) {
        boost::filesystem::path outP(inOutPath);
        if ( boost::filesystem::is_directory(outP) ) {
            boost::filesystem::path inP(inFile);
            outP /= inP.filename();
            outP.replace_extension(inOutExt);
        } else {
            outP = inOutPath;
        }
        outPath = outP.native();
    } else {
        boost::filesystem::path inP(inFile);
        boost::filesystem::path tmpPath(inP.parent_path());
        tmpPath /= inP.stem();
        tmpPath += "-copy";
        tmpPath.replace_extension(inOutExt);
        outPath = tmpPath.native();
    }
    BOOST_LOG_TRIVIAL(debug) << "Final Output Path: " << outPath;

    return outPath;
}

int main(int argc, char** argv) {
    InitBoostLog();
    BOOST_LOG_NAMED_SCOPE("main");
    
    try  { 
        std::vector<std::string>    inputFiles;

        /** Define and parse the program options 
         */ 
        namespace po = boost::program_options; 
        po::options_description desc("Options"); 
        desc.add_options() 
            ("help,h", "Print help messages") 
            ("output,o", po::value<std::string>(), "output file or folder")
            ("get,g", "Get the current open issues") 
            ("repo,r", po::value<std::string>(), "folder for the git repo to operate on")
            ("issues,i", po::value<std::string>(), "pre-downloaded/written comments")
            ("files", po::value<std::vector<std::string>>(&inputFiles), "input files")
            ;

        // this is how we setup `files` to work w/o the --files bit
        po::positional_options_description poFiles;
        poFiles.add("files", -1);

        po::variables_map vm; 
        try  { 
            // parse the CLI specifying our description & special positions
            po::store(po::command_line_parser(argc, argv).options(desc).positional(poFiles).run(), vm); 

            /** --help option
             *  OR no parameters...
             */ 
            if ( vm.count("help") || vm.size()==0 )  {
                std::cout << GetAppInfo() << std::endl << desc << std::endl;
                return SUCCESS; 
            } 

            // throws on error, so do after help in case
            // there are any problems 
            po::notify(vm);  
        } 
        catch(po::error& e)  { 
            std::cerr << "ERROR: " << e.what() << std::endl << std::endl; 
            std::cerr << desc << std::endl; 
            return ERROR_IN_COMMAND_LINE; 
        } 

        // application code here //
        BOOST_LOG_TRIVIAL(info) << "Welcome to " << GetAppInfo() << "!";
        
        std::string repoDir(".");    // current directory
        std::string issues;
        std::string outPath;

        if ( vm.count("repo") ) {
            repoDir = vm["repo"].as<std::string>();
            BOOST_LOG_TRIVIAL(debug) << "Repository: " << repoDir;
        }

        if ( vm.count("output") ) {
            outPath = vm["output"].as<std::string>();
            BOOST_LOG_TRIVIAL(debug) << "Output Path: " << outPath;
        }

        if ( vm.count("get") ) {
            bool doXLS = vm.count("xls")>0;
            if ( vm.count("issues")) {
                std::string issuesPath(vm["issues"].as<std::string>());
                issues = _ReadFile(issuesPath);
                outPath = _ComputeOutputPath(issuesPath, outPath, doXLS ? "xls" : "txt" );
            } else
                issues = _GetIssues(repoDir);

            if ( issues.length() ) {
                // BOOST_LOG_TRIVIAL(debug) << issues;
                if ( doXLS ) {

                } else if ( outPath.length() ) {
                    _WriteToFile( outPath, issues );
                } else {
                    std::cout << issues;
                }
            }
        } else if ( vm.count("files") ) {
            BOOST_LOG_TRIVIAL(info) << "Files: ";
            for ( auto f : inputFiles ) {
                BOOST_LOG_TRIVIAL(info) << "\t" << f;

                // and process!!
            }
        } else {
            BOOST_LOG_TRIVIAL(fatal) << "Unable to proceed with the specified options" << std::endl;
            return ERROR_IN_COMMAND_LINE;
        }
    } 
    catch(std::exception& e)  { 
        BOOST_LOG_TRIVIAL(fatal) << "Unhandled Exception reached the top of main: " 
                    << e.what() << ", application will now exit" << std::endl; 
        return ERROR_UNHANDLED_EXCEPTION; 
    }
    
    return SUCCESS; 
}
