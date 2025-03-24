//
// Asp application specification generator implementation.
//

#include "generator.h"
#include "symbol.hpp"
#include "symbols.h"
#include "appspec.h"
#include "data.h"
#include "crc.h"
#include <sstream>
#include <iomanip>

using namespace std;

static const string AppSpecVersion = "\x01";

static void WriteValue(ostream &, unsigned *specByteCount, const Literal &);
static void ContributeValue
    (const crc_spec_t &, crc_session_t &, const Literal &);

template <class T>
static void Write(ostream &os, T value)
{
    unsigned i = sizeof value;
    while (i--)
        os << static_cast<char>((value >> (i << 3)) & 0xFF);
}

static void WriteStringEscapedHexByte(ostream &os, uint8_t value)
{
    auto oldFlags = os.flags();
    auto oldFill = os.fill();

    os << hex << uppercase << setprecision(2) << setfill('0');
    if (value == 0)
        os << "\\0";
    else
        os << "\\x" << setw(2) << static_cast<unsigned>(value);

    os.flags(oldFlags);
    os.fill(oldFill);
}

template <class T>
static void WriteStringEscapedHex(ostream &os, T value)
{
    unsigned i = sizeof value;
    while (i--)
    {
        auto byte = static_cast<uint8_t>((value >> (i << 3)) & 0xFF);
        WriteStringEscapedHexByte(os, byte);
    }
}

void Generator::WriteCompilerSpec(ostream &os) const
{
    // Write the specification's header, including check value.
    os.write("AspS", 4);
    os.write(AppSpecVersion.c_str(), AppSpecVersion.size());
    Write(os, CheckValue());

    // Assign symbols, to variable and function names first, then to parameter
    // names, writing each name only once, in order of assigned symbol.
    for (const auto &definitionEntry: definitions)
    {
        const auto &name = definitionEntry.first;

        bool wasDefined = symbolTable.IsDefined(name);
        symbolTable.Symbol(name);

        if (!wasDefined)
            os << name << '\n';
    }
    for (const auto &definitionEntry: definitions)
    {
        const auto functionDefinition =
            dynamic_cast<const FunctionDefinition *>(definitionEntry.second);
        if (functionDefinition == nullptr)
            continue;

        const auto &parameters = functionDefinition->Parameters();

        for (auto parameterIter = parameters.ParametersBegin();
             parameterIter != parameters.ParametersEnd();
             parameterIter++)
        {
            const auto &parameter = **parameterIter;
            const auto &parameterName = parameter.Name();

            bool wasDefined = symbolTable.IsDefined(parameterName);
            symbolTable.Symbol(parameterName);

            if (!wasDefined)
                os << parameterName << '\n';
        }
    }
}

void Generator::WriteApplicationHeader(ostream &os) const
{
    os
        << "/*** AUTO-GENERATED; DO NOT EDIT ***/\n\n"
           "#ifndef ASP_APP_" << baseName << "_DEF_H\n"
           "#define ASP_APP_" << baseName << "_DEF_H\n\n"
           "#include <asp.h>\n\n"
           "#ifdef __cplusplus\n"
           "extern \"C\" {\n"
           "#endif\n\n"
           "extern AspAppSpec AspAppSpec_" << baseName << ";\n\n";

    // Write symbol macro definitions.
    for (auto iter = symbolTable.Begin();
         iter != symbolTable.End(); iter++)
    {
        const auto &name = iter->first;
        const auto &symbol = iter->second;

        os
            << "#define ASP_APP_" << baseName << "_SYM_" << name
            << ' ' << symbol << '\n';
    }

    // Write application function declarations.
    for (const auto &definitionEntry: definitions)
    {
        const auto &definition = definitionEntry.second;
        const auto functionDefinition =
            dynamic_cast<const FunctionDefinition *>(definition);
        if (functionDefinition == nullptr)
            continue;

        os << '\n';
        if (functionDefinition->IsLibraryInterface())
            os << "ASP_LIB_API ";
        os
            << "AspRunResult " << functionDefinition->InternalName() << "\n"
               "    (AspEngine *,";

        const auto &parameters = functionDefinition->Parameters();

        if (!parameters.ParametersEmpty())
            os << "\n";

        for (auto parameterIter = parameters.ParametersBegin();
             parameterIter != parameters.ParametersEnd();
             parameterIter++)
        {
            const auto &parameter = **parameterIter;

            os << "     AspDataEntry *" << parameter.Name() << ',';
            if (parameter.IsGroup())
                os
                    << " /* "
                    << (parameter.IsTupleGroup() ? "tuple" : "dictionary")
                    << " group */";
            os << '\n';
        }

        if (parameters.ParametersEmpty())
            os << ' ';
        else
            os << "     ";

        os << "AspDataEntry **returnValue);\n";
    }

    os
        << "\n"
           "#ifdef __cplusplus\n"
           "}\n"
           "#endif\n\n"
           "#endif\n";
}

void Generator::WriteApplicationCode(ostream &os) const
{
    os
        << "/*** AUTO-GENERATED; DO NOT EDIT ***/\n\n"
           "#include \"" << baseFileName << ".h\"\n"
           "#include <stdint.h>\n";

    // Write the dispatch function.
    os
        << "\nstatic AspRunResult AspDispatch_" << baseName
        << "\n    (AspEngine *engine, int32_t symbol, AspDataEntry *ns,\n"
           "     AspDataEntry **returnValue)\n"
           "{\n"
           "    switch (symbol)\n"
           "    {\n";
    for (const auto &definitionEntry: definitions)
    {
        const auto &definition = definitionEntry.second;
        const auto functionDefinition =
            dynamic_cast<const FunctionDefinition *>(definition);
        if (functionDefinition == nullptr)
            continue;

        auto symbol = symbolTable.Symbol(functionDefinition->Name());

        os
            << "        case " << symbol << ":\n"
            << "        {\n";

        const auto &parameters = functionDefinition->Parameters();

        for (auto parameterIter = parameters.ParametersBegin();
             parameterIter != parameters.ParametersEnd();
             parameterIter++)
        {
            const auto &parameter = **parameterIter;

            const auto &parameterName = parameter.Name();
            auto parameterSymbol = symbolTable.Symbol(parameterName);

            if (parameter.IsGroup())
            {
                os
                    << "            AspParameterResult " << parameterName
                    << " = AspGroupParameterValue(engine, ns, "
                    << parameterSymbol << ", "
                    << (parameter.IsTupleGroup() ? "false" : "true")
                    << ");\n"
                    << "            if (" << parameterName
                    << ".result != AspRunResult_OK)\n"
                    << "                return " << parameterName
                    << ".result;\n";
            }
            else
            {
                os
                    << "            AspDataEntry *" << parameterName
                    << " = AspParameterValue(engine, ns, "
                    << parameterSymbol << ");\n"
                    << "            if (" << parameterName << " == 0)\n"
                    << "                return AspRunResult_OutOfDataMemory;\n";
            }
        }

        os
            << "            return " << functionDefinition->InternalName()
            << "(engine, ";

        for (auto parameterIter = parameters.ParametersBegin();
             parameterIter != parameters.ParametersEnd();
             parameterIter++)
        {
            const auto &parameter = **parameterIter;

            os << parameter.Name();
            if (parameter.IsGroup())
                os << ".value";
            os << ", ";
        }

        os
            << "returnValue);\n"
               "        }\n";
    }
    os
        << "    }\n"
           "    return AspRunResult_UndefinedAppFunction;\n"
           "}\n";

    // Write the application specification structure.
    os
        << "\nAspAppSpec AspAppSpec_" << baseName << " =\n"
           "{";
    unsigned specByteCount = 0;
    for (const auto &definitionEntry: definitions)
    {
        const auto &definition = definitionEntry.second;
        const auto assignment =
            dynamic_cast<const Assignment *>(definition);
        const auto functionDefinition =
            dynamic_cast<const FunctionDefinition *>(definition);

        os << "\n    \"";

        if (assignment != nullptr)
        {
            const auto &value = assignment->Value();
            WriteStringEscapedHex
                (os,
                 static_cast<uint8_t>
                    (value == nullptr ?
                     AppSpecPrefix_Symbol : AppSpecPrefix_Variable));
            specByteCount++;

            if (value != nullptr)
                WriteValue(os, &specByteCount, *value);
        }
        else if (functionDefinition != nullptr)
        {
            auto &parameters = functionDefinition->Parameters();

            auto parameterCount = parameters.ParametersSize();
            if (parameterCount >
                static_cast<size_t>(AppSpecPrefix_MaxFunctionParameterCount))
            {
                ostringstream oss;
                oss
                    << "Function '" << functionDefinition->Name()
                    << "' has too many parameters ("
                    << parameterCount << " vs. max "
                    << AppSpecPrefix_MaxFunctionParameterCount << ')';
                throw oss.str();
            }
            WriteStringEscapedHex(os, static_cast<uint8_t>(parameterCount));
            specByteCount++;

            for (auto parameterIter = parameters.ParametersBegin();
                 parameterIter != parameters.ParametersEnd();
                 parameterIter++)
            {
                const auto &parameter = **parameterIter;

                int32_t parameterSymbol = symbolTable.Symbol(parameter.Name());
                auto word = *reinterpret_cast<uint32_t *>(&parameterSymbol);

                uint32_t parameterType = 0;
                const auto &defaultValue = parameter.DefaultValue();
                if (defaultValue != nullptr)
                    parameterType = AppSpecParameterType_Defaulted;
                else if (parameter.IsTupleGroup())
                    parameterType = AppSpecParameterType_TupleGroup;
                else if (parameter.IsDictionaryGroup())
                    parameterType = AppSpecParameterType_DictionaryGroup;
                word |= parameterType << AspWordBitSize;

                WriteStringEscapedHex(os, word);
                specByteCount += sizeof word;

                if (defaultValue != nullptr)
                    WriteValue(os, &specByteCount, *defaultValue);
            }
        }
        os << '"';
    }

    {
        auto oldFlags = os.flags();
        auto oldFill = os.fill();

        os
            << ",\n    " << specByteCount
            << hex << uppercase << setprecision(4) << setfill('0')
            << ", 0x" << setw(4) << CheckValue() << dec
            << ", AspDispatch_" << baseName << "\n"
               "};\n";

        os.flags(oldFlags);
        os.fill(oldFill);
    }
}

uint32_t Generator::CheckValue() const
{
    if (!checkValueComputed)
    {
        checkValue = ComputeCheckValue();
        checkValueComputed = true;
    }
    return checkValue;
}

uint32_t Generator::ComputeCheckValue() const
{
    // Use CRC-32/ISO-HDLC for computing a check value.
    const auto spec = crc_make_spec
        (32, 0x04C11DB7, 0xFFFFFFFF, true, true, 0xFFFFFFFF);
    crc_session_t session;
    crc_start(&spec, &session);

    // Contribute each definition to the check value.
    static const string
        CheckValueVariablePrefix = "\v",
        CheckValueFunctionPrefix = "\f",
        CheckValueParameterPrefix = "(";
    for (const auto &definitionEntry: definitions)
    {
        const auto &name = definitionEntry.first;
        const auto &definition = definitionEntry.second;
        const auto assignment =
            dynamic_cast<const Assignment *>(definition);
        const auto functionDefinition =
            dynamic_cast<const FunctionDefinition *>(definition);

        if (assignment != nullptr)
        {
            const auto &value = assignment->Value();

            // Contribute the variable name.
            crc_add
                (&spec, &session,
                 CheckValueVariablePrefix.c_str(),
                 static_cast<unsigned>(CheckValueVariablePrefix.size()));
            crc_add
                (&spec, &session,
                 name.c_str(), static_cast<unsigned>(name.size()));

            // Contribute the variable value if applicable.
            if (value != nullptr)
                ContributeValue(spec, session, *value);
        }
        else if (functionDefinition != nullptr)
        {
            // Contribute the function name.
            crc_add
                (&spec, &session,
                 CheckValueFunctionPrefix.c_str(),
                 static_cast<unsigned>(CheckValueFunctionPrefix.size()));
            crc_add
                (&spec, &session,
                 name.c_str(), static_cast<unsigned>(name.size()));

            const auto &parameters = functionDefinition->Parameters();

            for (auto parameterIter = parameters.ParametersBegin();
                 parameterIter != parameters.ParametersEnd();
                 parameterIter++)
            {
                const auto &parameter = **parameterIter;
                const auto &parameterName = parameter.Name();

                // Contribute the parameter name.
                crc_add
                    (&spec, &session,
                     CheckValueParameterPrefix.c_str(),
                     static_cast<unsigned>(CheckValueParameterPrefix.size()));
                crc_add
                    (&spec, &session,
                     parameterName.c_str(),
                     static_cast<unsigned>(parameterName.size()));

                // Contribute the default value if present.
                const auto &defaultValue = parameter.DefaultValue();
                if (defaultValue != nullptr)
                    ContributeValue(spec, session, *defaultValue);
            }
        }
        else
            throw string("Internal error");
    }
    return static_cast<uint32_t>(crc_finish(&spec, &session));
}

static void WriteValue
    (ostream &os, unsigned *specByteCount, const Literal &literal)
{
    auto valueType = literal.GetType();
    WriteStringEscapedHex(os, static_cast<uint8_t>(valueType));
    (*specByteCount)++;

    switch (valueType)
    {
        case AppSpecValueType_Boolean:
        {
            auto value = literal.BooleanValue();
            WriteStringEscapedHex
                (os, static_cast<uint8_t>(value));
            (*specByteCount)++;
            break;
        }

        case AppSpecValueType_Integer:
        {
            int32_t value = literal.IntegerValue();
            WriteStringEscapedHex
                (os, *reinterpret_cast<uint32_t *>(&value));
            *specByteCount += sizeof value;
            break;
        }

        case AppSpecValueType_Float:
        {
            static const uint16_t word = 1;
            bool be = *(const char *)&word == 0;

            auto value = literal.FloatValue();
            auto data = reinterpret_cast<const uint8_t *>(&value);
            for (unsigned i = 0; i < sizeof value; i++)
            {
                auto b = data[be ? i : sizeof value - 1 - i];
                WriteStringEscapedHex(os, b);
            }
            *specByteCount += sizeof value;
            break;
        }

        case AppSpecValueType_String:
        {
            auto value = literal.StringValue();
            auto valueSize = static_cast<uint32_t>
                (value.size());
            WriteStringEscapedHex(os, valueSize);
            for (unsigned i = 0; i < valueSize; i++)
                WriteStringEscapedHex
                    (os, static_cast<uint8_t>(value[i]));
            *specByteCount += sizeof(valueSize) + valueSize;
            break;
        }
    }
}

static void ContributeValue
    (const crc_spec_t &spec, crc_session_t &session, const Literal &literal)
{
    auto valueType = literal.GetType();
    auto b = static_cast<uint8_t>(valueType);
    crc_add(&spec, &session, &b, 1);

    switch (valueType)
    {
        case AppSpecValueType_Boolean:
        {
            uint8_t b = literal.BooleanValue() ? 1 : 0;
            crc_add(&spec, &session, &b, 1);
            break;
        }

        case AppSpecValueType_Integer:
        {
            int32_t value = literal.IntegerValue();
            auto uValue = *reinterpret_cast<uint32_t *>(&value);
            for (unsigned i = 0; i < sizeof value; i++)
            {
                uint8_t b =
                    uValue >> 8 * (sizeof value - 1 - i) & 0xFF;
                crc_add(&spec, &session, &b, 1);
            }
            break;
        }

        case AppSpecValueType_Float:
        {
            static const uint16_t word = 1;
            bool be = *(const char *)&word == 0;

            auto value = literal.FloatValue();
            auto data = reinterpret_cast<const uint8_t *>(&value);
            for (unsigned i = 0; i < sizeof value; i++)
            {
                auto b = data[be ? i : sizeof value - 1 - i];
                crc_add(&spec, &session, &b, 1);
            }
            break;
        }

        case AppSpecValueType_String:
        {
            const auto &s = literal.StringValue();
            crc_add(&spec, &session, s.c_str(), s.size());
        }
    }
}
