//
// Standalone Asp application main.
//

#include "asp.h"
#include "asp-info.h"
#include "standalone.h"
#include "context.h"
#include <ctime>
#include <csignal>
#include <iostream>
#include <iomanip>
#include <set>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <climits>

#if !defined ASP_STANDALONE_VERSION_MAJOR || \
    !defined ASP_STANDALONE_VERSION_MINOR || \
    !defined ASP_STANDALONE_VERSION_PATCH || \
    !defined ASP_STANDALONE_VERSION_TWEAK
#error ASP_STANDALONE_VERSION_* macros undefined
#endif
#ifndef COMMAND_OPTION_PREFIXES
#error COMMAND_OPTION_PREFIXES macro undefined
#endif

using namespace std;

static const size_t DEFAULT_DATA_ENTRY_COUNT = 2048;

static AspRunResult LoadCodePage
    (void *, uint32_t offset, size_t *size, void *codePage);
static void HandleInterrupt(int);
static bool Interrupted = false;

static void Usage()
{
    cerr
        << "Usage:      asps [OPTION]... ["
        << COMMAND_OPTION_PREFIXES[0] << COMMAND_OPTION_PREFIXES[0]
        << "] SCRIPT [ARG]...\n"
        << "\n"
        << "Run the Asp script executable SCRIPT (*.aspe)."
        << " The suffix may be omitted.\n"
        << "If one or more ARG are given,"
        << " they are passed as arguments to the script.\n"
        << "\n"
        << "Use " << COMMAND_OPTION_PREFIXES[0] << COMMAND_OPTION_PREFIXES[0]
        << " before the SCRIPT argument if it starts with an option prefix.\n"
        << "\n"
        << "Options";
    if (strlen(COMMAND_OPTION_PREFIXES) > 1)
    {
        cerr << " (may be prefixed by";
        for (unsigned i = 1; i < strlen(COMMAND_OPTION_PREFIXES); i++)
        {
            if (i != 1)
                cerr << ',';
            cerr << ' ' << COMMAND_OPTION_PREFIXES[i];
        }
        cerr << " instead of " << COMMAND_OPTION_PREFIXES[0] << ')';
    }
    cerr
        << ":\n"
        << COMMAND_OPTION_PREFIXES[0]
        << "c n        Code size, in bytes."
        << " The default behaviour is to determine the size\n"
        << "            from the SCRIPT file."
        << " This default behaviour may also be invoked\n"
        << "            explicitly by specifying 0 for n.\n"
        << COMMAND_OPTION_PREFIXES[0]
        << "d n        Data entry count, where each entry is "
        << AspDataEntrySize() << " bytes."
        << " Default is " << DEFAULT_DATA_ENTRY_COUNT << ".\n"
        << COMMAND_OPTION_PREFIXES[0]
        << "h          Print usage information and exit.\n"
        #ifdef ASP_DEBUG
        << COMMAND_OPTION_PREFIXES[0]
        << "n n        Number of instructions to execute before exiting."
        << " Useful for\n"
        << "            debugging. Available only in debug builds.\n"
        #endif
        << COMMAND_OPTION_PREFIXES[0]
        << "p n        Code page size, in bytes. The default is 0, which"
        << " disables paging\n"
        << "            mode. The number of pages is this value divided by the"
        << " code size.\n"
        #ifdef ASP_DEBUG
        << COMMAND_OPTION_PREFIXES[0]
        << "t file     Trace output file."
        << " The default is standard output. Overrides a\n"
        << "            previous "
        << COMMAND_OPTION_PREFIXES[0] << "t or "
        << COMMAND_OPTION_PREFIXES[0] << "T option.\n"
        << COMMAND_OPTION_PREFIXES[0]
        << "T fd       Trace output file descriptor."
        << " Only 1 or 2 may be specified. The\n"
        << "            default is 1 (standard output)."
        << " Overrides a previous "
        << COMMAND_OPTION_PREFIXES[0] << "t or "
        << COMMAND_OPTION_PREFIXES[0] << "T\n"
        << "            option.\n"
        << COMMAND_OPTION_PREFIXES[0]
        << "u file     Data memory dump output file."
        << " The default is standard output.\n"
        << "            Overrides a previous "
        << COMMAND_OPTION_PREFIXES[0] << "u or "
        << COMMAND_OPTION_PREFIXES[0] << "U option.\n"
        << COMMAND_OPTION_PREFIXES[0]
        << "U fd       Data memory dump output file descriptor."
        << " Only 1 or 2 may be\n"
        << "            specified. The default is 1 (standard output)."
        << " Overrides a previous\n"
        << "            " << COMMAND_OPTION_PREFIXES[0] << "u or "
        << COMMAND_OPTION_PREFIXES[0] << "U option.\n"
        #endif
        << COMMAND_OPTION_PREFIXES[0]
        << "v          Verbose. Output version and statistical information.\n"
        ;
}

template<class C> static void CloseFiles(C files)
{
    bool warned = false;
    for (auto file: files)
    {
        if (fclose(file) != 0 && !warned)
        {
            cerr << "Error closing one or more output files" << endl;
            warned = true;
        }
    }
}

int main(int argc, char **argv)
{
    // Process command line options.
    bool verbose = false;
    size_t codeByteCount = 0, codePageByteCount = 0;
    size_t dataEntryCount = DEFAULT_DATA_ENTRY_COUNT;
    #ifdef ASP_DEBUG
    unsigned stepCountLimit = UINT_MAX;
    string traceFileName, dumpFileName;
    int traceFileDescriptor = 0, dumpFileDescriptor = 0;
    #endif
    for (; argc >= 2; argc--, argv++)
    {
        string arg1 = argv[1];
        string prefix = arg1.substr(0, 1);
        auto prefixIndex =
            string(COMMAND_OPTION_PREFIXES).find_first_of(prefix);
        if (prefixIndex == string::npos)
            break;
        string option = arg1.substr(1);
        if (option == string(1, COMMAND_OPTION_PREFIXES[prefixIndex]))
        {
            argc--; argv++;
            break;
        }

        if (option == "?" || option == "h")
        {
            Usage();
            return 0;
        }
        else if (option == "c")
        {
            string value = (++argv)[1];
            argc--;
            codeByteCount = static_cast<size_t>(atoi(value.c_str()));
        }
        else if (option == "d")
        {
            string value = (++argv)[1];
            argc--;
            dataEntryCount = static_cast<size_t>(atoi(value.c_str()));
        }
        else if (option == "p")
        {
            string value = (++argv)[1];
            argc--;
            codePageByteCount = static_cast<size_t>(atoi(value.c_str()));
        }
        #ifdef ASP_DEBUG
        else if (option == "n")
        {
            string value = (++argv)[1];
            argc--;
            stepCountLimit = static_cast<unsigned>(atoi(value.c_str()));
        }
        else if (option == "t")
        {
            traceFileName = (++argv)[1];
            argc--;
            traceFileDescriptor = 0;
        }
        else if (option == "T")
        {
            string value = (++argv)[1];
            int fd = atoi(value.c_str());
            argc--;
            if (fd != 1 && fd != 2)
            {
                cerr << "Invalid trace file descriptor " << value << endl;
                return 1;
            }
            traceFileDescriptor = fd;
            traceFileName.clear();
        }
        else if (option == "u")
        {
            dumpFileName = (++argv)[1];
            argc--;
            dumpFileDescriptor = 0;
        }
        else if (option == "U")
        {
            string value = (++argv)[1];
            int fd = atoi(value.c_str());
            argc--;
            if (fd != 1 && fd != 2)
            {
                cerr << "Invalid dump file descriptor " << value << endl;
                return 1;
            }
            dumpFileDescriptor = fd;
            dumpFileName.clear();
        }
        #endif
        else if (option == "v")
            verbose = true;
        else
        {
            cerr << "Invalid option: " << arg1 << endl;
            Usage();
            return 1;
        }
    }

    // Prepare to close files when done.
    set<FILE *> openedFiles;

    // Open the trace and dump files.
    #ifdef ASP_DEBUG
    FILE *stdFiles[] = {nullptr, stdout, stderr};
    FILE *traceFile = stdout;
    if (traceFileDescriptor != 0)
        traceFile = stdFiles[traceFileDescriptor];
    else if (!traceFileName.empty())
    {
        traceFile = fopen(traceFileName.c_str(), "w");
        if (traceFile == nullptr)
        {
            cerr
                << "Error opening trace file " << traceFileName
                << ": " << strerror(errno) << endl;
            CloseFiles(openedFiles);
            return 1;
        }
        openedFiles.insert(traceFile);
    }
    FILE *dumpFile = stdout;
    if (dumpFileDescriptor != 0)
        dumpFile = stdFiles[dumpFileDescriptor];
    else if (!dumpFileName.empty())
    {
        if (dumpFileName == traceFileName)
            dumpFile = traceFile;
        else
        {
            dumpFile = fopen(dumpFileName.c_str(), "w");
            if (dumpFile == nullptr)
            {
                cerr
                    << "Error opening dump file " << dumpFileName
                    << ": " << strerror(errno) << endl;
                CloseFiles(openedFiles);
                return 1;
            }
            openedFiles.insert(traceFile);
        }
    }
    #endif

    // Report program version information.
    FILE *reportFile;
    #ifdef ASP_DEBUG
    reportFile = traceFile;
    #else
    reportFile = stdout;
    #endif
    if (verbose)
    {
        fprintf
            (reportFile, "Asp standalone application version %d.%d.%d.%d\n",
             ASP_STANDALONE_VERSION_MAJOR,
             ASP_STANDALONE_VERSION_MINOR,
             ASP_STANDALONE_VERSION_PATCH,
             ASP_STANDALONE_VERSION_TWEAK);
    }

    // Obtain executable file name.
    if (argc < 2)
    {
        // Exit gracefully if verbose mode is specified.
        if (verbose)
            return 0;

        cerr << "No program specified" << endl;
        Usage();
        return 1;
    }

    // Open the executable file.
    string executableFileName = argv[1];
    auto openExecutable = [](const char *fileName)
    {
        return fopen(fileName, "rb");
    };
    FILE *executableFile = openExecutable(executableFileName.c_str());
    static const string executableSuffix = ".aspe";
    if (executableFile == nullptr)
    {
        // Try appending the appropriate suffix if the specified file did not
        // exist.
        executableFileName += executableSuffix;
        executableFile = openExecutable(executableFileName.c_str());
    }
    if (executableFile == nullptr)
    {
        cerr
            << "Error opening " << executableFileName
            << ": " << strerror(errno) << endl;
        return 1;
    }
    openedFiles.insert(executableFile);

    // Determine byte size of data area.
    size_t dataEntrySize = AspDataEntrySize();
    size_t dataByteSize = dataEntryCount * dataEntrySize;

    // Initialize the Asp engine.
    StandaloneAspContext context;
    AspEngine engine;
    auto code = unique_ptr<char[]>();
    if (codeByteCount != 0)
    {
        code.reset(new char[codeByteCount]);
        if (code == nullptr)
        {
            cerr << "Error allocating engine code area" << endl;
            CloseFiles(openedFiles);
            return 2;
        }
    }
    auto data = unique_ptr<char[]>(new char [dataByteSize]);
    if (data == nullptr)
    {
        cerr << "Error allocating engine data area" << endl;
        CloseFiles(openedFiles);
        return 2;
    }
    AspRunResult initializeResult = AspInitialize
        (&engine,
         code.get(), codeByteCount,
         data.get(), dataByteSize,
         &AspAppSpec_standalone, &context);
    if (initializeResult != AspRunResult_OK)
    {
        cerr
            << "Initialize error 0x" << hex << uppercase << setfill('0')
            << setw(2) << initializeResult << ": "
            << AspRunResultToString(static_cast<int>(initializeResult))
            << endl;
        CloseFiles(openedFiles);
        return 2;
    }

    // Assign the trace output file.
    #ifdef ASP_DEBUG
    AspTraceFile(&engine, traceFile);
    #endif

    // Load the executable using one of three methods.
    auto externalCode = unique_ptr<char[]>();
    if (codeByteCount == 0)
    {
        if (codePageByteCount != 0)
        {
            cerr << "WARNING: Code page size ignored" << endl;
            codePageByteCount = 0;
        }

        // Determine the size of the executable file.
        int seekResult = fseek(executableFile, 0, SEEK_END);
        long tellResult = 0;
        if (seekResult == 0)
            tellResult = ftell(executableFile);
        if (seekResult != 0 || tellResult < 0)
        {
            cerr
                << "Error determining size of " << executableFileName
                << ": " << strerror(errno) << endl;
            CloseFiles(openedFiles);
            return 2;
        }
        auto externalCodeSize = static_cast<size_t>(tellResult);
        externalCode.reset(new char[externalCodeSize]);
        if (externalCode == nullptr)
        {
            cerr << "Error allocating memory for executable code" << endl;
            CloseFiles(openedFiles);
            return 2;
        }
        rewind(executableFile);

        // Read the entire executable into memory.
        size_t readResult = fread
            (externalCode.get(), externalCodeSize, 1U, executableFile);
        if (readResult != 1U || feof(executableFile) || ferror(executableFile))
        {
            cerr
                << "Error reading " << executableFileName
                << ": " << strerror(errno) << endl;
            CloseFiles(openedFiles);
            return 2;
        }
        openedFiles.erase(executableFile);
        fclose(executableFile);
        executableFile = nullptr;

        AspAddCodeResult sealResult = AspSealCode
            (&engine, externalCode.get(), externalCodeSize);
        if (sealResult != AspAddCodeResult_OK)
        {
            cerr
                << "Seal error 0x" << hex << uppercase << setfill('0')
                << setw(2) << sealResult << ": "
                << AspAddCodeResultToString(static_cast<int>(sealResult))
                << endl;
            CloseFiles(openedFiles);
            return 2;
        }
    }
    else if (codePageByteCount == 0)
    {
        while (true)
        {
            auto c = static_cast<char>(fgetc(executableFile));
            if (feof(executableFile))
                break;
            if (ferror(executableFile))
            {
                cerr
                    << "Error reading " << executableFileName
                    << ": " << strerror(errno) << endl;
                CloseFiles(openedFiles);
                return 2;
            }
            AspAddCodeResult addResult = AspAddCode(&engine, &c, 1);
            if (addResult != AspAddCodeResult_OK)
            {
                cerr
                    << "Load error 0x" << hex << uppercase << setfill('0')
                    << setw(2) << addResult << ": "
                    << AspAddCodeResultToString(static_cast<int>(addResult))
                    << endl;
                CloseFiles(openedFiles);
                return 2;
            }
        }
        openedFiles.erase(executableFile);
        fclose(executableFile);
        executableFile = nullptr;

        AspAddCodeResult sealResult = AspSeal(&engine);
        if (sealResult != AspAddCodeResult_OK)
        {
            cerr
                << "Seal error 0x" << hex << uppercase << setfill('0')
                << setw(2) << sealResult << ": "
                << AspAddCodeResultToString(static_cast<int>(sealResult))
                << endl;
            CloseFiles(openedFiles);
            return 2;
        }
    }
    else
    {
        size_t computedCodePageCount = codeByteCount / codePageByteCount;
        if (computedCodePageCount == 0)
        {
            cerr
                << "Code page size is larger than available code area."
                << endl;
            CloseFiles(openedFiles);
            return 2;
        }
        auto codePageCount = static_cast<uint8_t>(computedCodePageCount);
        if (codePageCount != computedCodePageCount)
            cerr
                << "WARNING: Number of code pages limited to "
                << static_cast<unsigned>(codePageCount) << endl;

        AspRunResult setPagingResult = AspSetCodePaging
            (&engine, codePageCount, codePageByteCount, LoadCodePage);
        if (setPagingResult != AspRunResult_OK)
        {
            cerr
                << "Error 0x" << hex << uppercase << setfill('0')
                << setw(2) << setPagingResult << " initializing code paging: "
                << AspRunResultToString(static_cast<int>(setPagingResult))
                << endl;
            CloseFiles(openedFiles);
            return 2;
        }

        AspAddCodeResult pageResult = AspPageCode(&engine, executableFile);
        if (pageResult != AspAddCodeResult_OK)
        {
            cerr
                << "Error 0x" << hex << uppercase << setfill('0')
                << setw(2) << pageResult << " loading paged code: "
                << AspAddCodeResultToString(static_cast<int>(pageResult))
                << endl;
            CloseFiles(openedFiles);
            return 2;
        }
    }

    // Report engine and code version information.
    if (verbose)
    {
        uint8_t engineVersion[4], codeVersion[4];
        AspEngineVersion(engineVersion);
        AspCodeVersion(&engine, codeVersion);
        fputs("Engine version: ", reportFile);
        for (unsigned i = 0; i < sizeof engineVersion; i++)
        {
            if (i != 0)
                fputc('.', reportFile);
            fprintf(reportFile, "%u", static_cast<unsigned>(engineVersion[i]));
        }
        fputs("\nCode version: ", reportFile);
        for (unsigned i = 0; i < sizeof codeVersion; i++)
        {
            if (i != 0)
                fputc('.', reportFile);
            fprintf(reportFile, "%u", static_cast<unsigned>(codeVersion[i]));
        }
        fputc('\n', reportFile);
    }

    // Set arguments.
    AspRunResult argumentResult = AspSetArguments(&engine, argv + 2);
    if (argumentResult != AspRunResult_OK)
    {
        cerr
            << "Arguments error 0x" << hex << uppercase << setfill('0')
            << setw(2) << argumentResult << ": "
            << AspRunResultToString(static_cast<int>(argumentResult))
            << endl;
        CloseFiles(openedFiles);
        return 2;
    }

    // Prepare for the run for the potential of being interrupted by the user.
    signal(SIGINT, HandleInterrupt);

    // Run the code.
    context.sleeping = false;
    AspRunResult runResult = AspRunResult_OK;
    unsigned stepCount = 0;
    #ifdef ASP_DEBUG
    if (stepCountLimit == UINT_MAX)
        fputs("Executing instructions indefinitely...\n", reportFile);
    else
        fprintf(reportFile, "Executing %u instructions...\n", stepCountLimit);
    #endif
    for (;
         !Interrupted && runResult == AspRunResult_OK
         #ifdef ASP_DEBUG
         && (stepCountLimit == UINT_MAX || stepCount < stepCountLimit)
         #endif
         ; stepCount++)
    {
        runResult = AspStep(&engine);
        if (context.sleeping)
        {
            while (clock() < context.expiry) ;
            context.sleeping = false;
        }
    }

    // Close the executable if not already done (e.g., in code paging mode).
    if (executableFile != nullptr)
    {
        openedFiles.erase(executableFile);
        fclose(executableFile);
        executableFile = nullptr;
    }

    // Dump data area in debug mode.
    #ifdef ASP_DEBUG
    if (dumpFile == traceFile)
        fputs("---\n", dumpFile);
    fputs("Dump:\n", dumpFile);
    AspDump(&engine, dumpFile);
    #endif

    #ifdef ASP_DEBUG
    fprintf(reportFile, "---\nExecuted %u instructions\n", stepCount);
    if (stepCountLimit != UINT_MAX && stepCount != stepCountLimit)
        fprintf
            (reportFile,
             "WARNING: Did not execute specified number of instructions (%u)\n",
             stepCountLimit);
    #endif

    // Check completion status of the run.
    FILE *statusFile;
    #ifdef ASP_DEBUG
    statusFile = traceFile;
    #else
    statusFile = stderr;
    if (runResult != AspRunResult_Complete)
    #endif
    {
        if (Interrupted)
            fputs("Run was INTERRUPTED!\n", reportFile);
        if (runResult != AspRunResult_OK)
            fprintf
                (statusFile,
                 "Run error 0x%02X: %s\n",
                 runResult, AspRunResultToString(static_cast<int>(runResult)));

        // Report the program counter.
        auto programCounter = AspProgramCounter(&engine);
        fprintf(statusFile, "Program counter: 0x%07zX", programCounter);

        // Attempt to translate the program counter into a source location
        // using the associated source info file.
        size_t suffixPos =
            executableFileName.size() - executableSuffix.size();
        static const string sourceInfoSuffix = ".aspd";
        string sourceInfoFileName =
            executableFileName.substr(0, suffixPos) + sourceInfoSuffix;
        AspSourceInfo *sourceInfo = AspLoadSourceInfoFromFile
            (sourceInfoFileName.c_str());
        if (sourceInfo != nullptr)
        {
            AspSourceLocation sourceLocation = AspGetSourceLocation
                (sourceInfo, programCounter);
            if (sourceLocation.fileName != nullptr)
            {
                fprintf
                    (statusFile, "; %s:%u:%u",
                     sourceLocation.fileName,
                     sourceLocation.line, sourceLocation.column);
            }
            AspUnloadSourceInfo(sourceInfo);
        }
        fputc('\n', statusFile);
    }

    // Report low free count.
    if (verbose)
    {
        fprintf
            (reportFile, "Low free count: %zu (max %zu)\n",
             AspLowFreeCount(&engine), AspMaxDataSize(&engine));
        if (codePageByteCount != 0)
        {
            fprintf
                (reportFile, "Code page read count: %zu\n",
                 AspCodePageReadCount(&engine, false));
        }
    }

    CloseFiles(openedFiles);

    return runResult == AspRunResult_Complete ? 0 : 2;
}

static AspRunResult LoadCodePage
    (void *id, uint32_t offset, size_t *size, void *codePage)
{
    auto executableFile = static_cast<FILE *>(id);

    int result = fseek(executableFile, (long)offset, SEEK_SET);
    if (result != 0)
        return ferror(executableFile) != 0 ?
            AspRunResult_Application : AspRunResult_BeyondEndOfCode;

    size_t readCount = fread(codePage, 1, *size, executableFile);
    if (ferror(executableFile) != 0)
        return AspRunResult_Application;
    *size = readCount;

    return AspRunResult_OK;
}

static void HandleInterrupt(int)
{
    Interrupted = true;
}
