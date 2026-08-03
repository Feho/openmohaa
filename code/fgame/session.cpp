// session.cpp: Process-lifetime script variables.

#include "session.h"

#include "g_local.h"
#include "scriptmaster.h"
#include "../script/scriptexception.h"
#include "../script/scriptvariable.h"

#include <bit>
#include <charconv>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

    constexpr char kSessionCvar[]  = "_openmohaa_script_session";
    constexpr char kFormatHeader[] = "OMS1";

    class SessionEncoder
    {
    public:
        std::string Encode(ScriptVariable& value)
        {
            std::string output;
            EncodeValue(value, output);
            return output;
        }

    private:
        std::unordered_set<const ScriptArrayHolder *> activeArrays;

        static void AppendUnsigned(uint32_t value, std::string& output)
        {
            char       buffer[16];
            const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);

            if (result.ec != std::errc()) {
                throw std::runtime_error("could not encode an unsigned integer");
            }

            output.append(buffer, result.ptr);
        }

        static void AppendInteger(int value, std::string& output)
        {
            char       buffer[16];
            const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);

            if (result.ec != std::errc()) {
                throw std::runtime_error("could not encode an integer");
            }

            output.append(buffer, result.ptr);
            output.push_back(';');
        }

        static void AppendFloat(float value, std::string& output)
        {
            constexpr char digits[] = "0123456789abcdef";
            const uint32_t bits     = std::bit_cast<uint32_t>(value);

            for (int shift = 28; shift >= 0; shift -= 4) {
                output.push_back(digits[(bits >> shift) & 0xf]);
            }
        }

        static void AppendString(const str& value, std::string& output)
        {
            AppendUnsigned(value.length(), output);
            output.push_back(':');
            output.append(value.c_str(), value.length());
        }

        void EncodeArray(ScriptVariable& value, std::string& output)
        {
            ScriptArrayHolder *const holder = value.m_data.arrayValue;

            if (!activeArrays.insert(holder).second) {
                throw std::runtime_error("cyclic arrays are not supported");
            }

            AppendUnsigned(holder->arrayValue.size(), output);
            output.push_back(':');

            con_map_enum<ScriptVariable, ScriptVariable> entries(holder->arrayValue);
            for (ScriptVariable *key = entries.NextKey(); key; key = entries.NextKey()) {
                EncodeValue(*key, output);
                EncodeValue(*entries.CurrentValue(), output);
            }

            activeArrays.erase(holder);
        }

        void EncodeValue(ScriptVariable& value, std::string& output)
        {
            switch (value.GetType()) {
            case VARIABLE_NONE:
                output.push_back('N');
                break;
            case VARIABLE_STRING:
                output.push_back('S');
                AppendString(value.stringValue(), output);
                break;
            case VARIABLE_CONSTSTRING:
                output.push_back('T');
                AppendString(value.stringValue(), output);
                break;
            case VARIABLE_INTEGER:
                output.push_back('I');
                AppendInteger(value.intValue(), output);
                break;
            case VARIABLE_FLOAT:
                output.push_back('F');
                AppendFloat(value.floatValue(), output);
                break;
            case VARIABLE_CHAR:
                output.push_back('C');
                AppendInteger(value.charValue(), output);
                break;
            case VARIABLE_VECTOR:
                {
                    const Vector vector = value.vectorValue();
                    output.push_back('V');
                    AppendFloat(vector[0], output);
                    AppendFloat(vector[1], output);
                    AppendFloat(vector[2], output);
                    break;
                }
            case VARIABLE_ARRAY:
                output.push_back('A');
                EncodeArray(value, output);
                break;
            case VARIABLE_CONSTARRAY:
                output.push_back('Q');
                AppendUnsigned(value.arraysize(), output);
                output.push_back(':');
                for (int index = 1; index <= value.arraysize(); ++index) {
                    EncodeValue(*value[index], output);
                }
                break;
            default:
                throw std::runtime_error(std::string("unsupported value type '") + value.GetTypeName() + "'");
            }
        }
    };

    class SessionDecoder
    {
    public:
        explicit SessionDecoder(std::string_view input)
            : input(input)
        {}

        bool AtEnd() const { return position == input.size(); }

        std::string ReadString()
        {
            const uint32_t length = ReadUnsigned(':');

            if (length > input.size() - position) {
                throw std::runtime_error("truncated string");
            }

            const std::string value(input.substr(position, length));
            position += length;
            return value;
        }

        uint32_t ReadCount() { return ReadUnsigned(':'); }

        void ValidateCount(uint32_t count, size_t minimumBytesPerItem) const
        {
            if (count > (input.size() - position) / minimumBytesPerItem) {
                throw std::runtime_error("invalid item count");
            }
        }

        ScriptVariable ReadValue()
        {
            const char     type = ReadChar();
            ScriptVariable value;

            switch (type) {
            case 'N':
                break;
            case 'S':
                value.setStringValue(ReadString().c_str());
                break;
            case 'T':
                value.setConstStringValue(Director.AddString(ReadString().c_str()));
                break;
            case 'I':
                value.setIntValue(ReadInteger());
                break;
            case 'F':
                value.setFloatValue(ReadFloat());
                break;
            case 'C':
                value.setCharValue(static_cast<char>(ReadInteger()));
                break;
            case 'V':
                {
                    const float x = ReadFloat();
                    const float y = ReadFloat();
                    const float z = ReadFloat();
                    value.setVectorValue(Vector(x, y, z));
                    break;
                }
            case 'A':
                {
                    value.type              = VARIABLE_ARRAY;
                    value.m_data.arrayValue = new ScriptArrayHolder;

                    const uint32_t count = ReadCount();
                    ValidateCount(count, 2);
                    for (uint32_t index = 0; index < count; ++index) {
                        ScriptVariable key       = ReadValue();
                        ScriptVariable itemValue = ReadValue();
                        value.setArrayAtRef(key, itemValue);
                    }
                    break;
                }
            case 'Q':
                {
                    const uint32_t count = ReadCount();
                    ValidateCount(count, 1);
                    std::vector<ScriptVariable> values(count);

                    for (ScriptVariable& itemValue : values) {
                        itemValue = ReadValue();
                    }

                    value.setConstArrayValue(values.data(), count);
                    break;
                }
            default:
                throw std::runtime_error("unknown value type");
            }

            return value;
        }

    private:
        std::string_view input;
        size_t           position = 0;

        char ReadChar()
        {
            if (AtEnd()) {
                throw std::runtime_error("unexpected end of data");
            }

            return input[position++];
        }

        uint32_t ReadUnsigned(char terminator)
        {
            const size_t start = position;
            while (!AtEnd() && input[position] != terminator) {
                ++position;
            }

            if (AtEnd() || start == position) {
                throw std::runtime_error("invalid unsigned integer");
            }

            uint32_t   value;
            const auto result = std::from_chars(input.data() + start, input.data() + position, value);
            if (result.ec != std::errc() || result.ptr != input.data() + position) {
                throw std::runtime_error("invalid unsigned integer");
            }

            ++position;
            return value;
        }

        int ReadInteger()
        {
            const size_t start = position;
            while (!AtEnd() && input[position] != ';') {
                ++position;
            }

            if (AtEnd() || start == position) {
                throw std::runtime_error("invalid integer");
            }

            int        value;
            const auto result = std::from_chars(input.data() + start, input.data() + position, value);
            if (result.ec != std::errc() || result.ptr != input.data() + position) {
                throw std::runtime_error("invalid integer");
            }

            ++position;
            return value;
        }

        float ReadFloat()
        {
            if (input.size() - position < 8) {
                throw std::runtime_error("truncated float");
            }

            uint32_t   bits;
            const auto result = std::from_chars(input.data() + position, input.data() + position + 8, bits, 16);
            if (result.ec != std::errc() || result.ptr != input.data() + position + 8) {
                throw std::runtime_error("invalid float");
            }

            position += 8;
            return std::bit_cast<float>(bits);
        }
    };

} // namespace

Session session;

void Session::PrepareForScriptReset()
{
    if (!restored) {
        return;
    }

    Save();
    restored = false;
}

void Session::Restore()
{
    if (restored) {
        return;
    }

    restored = true;
    Vars()->ClearList();

    const cvar_t *const    storage = gi.Cvar_Get(kSessionCvar, "", CVAR_ROM | CVAR_NORESTART);
    const std::string_view data    = storage->string;

    if (data.empty()) {
        return;
    }

    const auto restoreFailed = [this](const char *message)
    {
        Vars()->ClearList();
        gi.cvar_set(kSessionCvar, "");
        gi.Printf("^1Could not restore session variables: %s\n", message);
    };

    try {
        if (!data.starts_with(kFormatHeader)) {
            throw std::runtime_error("unknown format version");
        }

        SessionDecoder decoder(data.substr(sizeof(kFormatHeader) - 1));
        const uint32_t count = decoder.ReadCount();
        decoder.ValidateCount(count, 3);

        for (uint32_t index = 0; index < count; ++index) {
            const std::string name  = decoder.ReadString();
            ScriptVariable    value = decoder.ReadValue();
            Vars()->SetVariable(Director.AddString(name.c_str()), std::move(value));
        }

        if (!decoder.AtEnd()) {
            throw std::runtime_error("trailing data");
        }
    } catch (const ScriptException& error) {
        restoreFailed(error.string.c_str());
    } catch (const std::exception& error) {
        restoreFailed(error.what());
    }
}

void Session::Save()
{
    if (!restored) {
        return;
    }

    std::vector<std::pair<std::string, std::string>> variables;
    con_set_enum<short3, ScriptVariable>             entries = Vars()->list;

    for (auto *entry = entries.NextElement(); entry; entry = entries.NextElement()) {
        str variableName;

        try {
            SessionEncoder encoder;
            variableName = entry->value.getName();
            variables.emplace_back(variableName.c_str(), encoder.Encode(entry->value));
        } catch (const ScriptException& error) {
            gi.Printf(
                "^1Not persisting session.%s: %s\n",
                variableName.length() ? variableName.c_str() : "<unknown>",
                error.string.c_str()
            );
        } catch (const std::exception& error) {
            gi.Printf(
                "^1Not persisting session.%s: %s\n",
                variableName.length() ? variableName.c_str() : "<unknown>",
                error.what()
            );
        }
    }

    std::string output(kFormatHeader);
    {
        char       buffer[16];
        const auto result = std::to_chars(std::begin(buffer), std::end(buffer), variables.size());
        output.append(buffer, result.ptr);
        output.push_back(':');
    }

    for (const auto& [name, value] : variables) {
        char       buffer[16];
        const auto result = std::to_chars(std::begin(buffer), std::end(buffer), name.size());
        output.append(buffer, result.ptr);
        output.push_back(':');
        output.append(name);
        output.append(value);
    }

    gi.cvar_set(kSessionCvar, output.c_str());
}

CLASS_DECLARATION(Listener, Session, NULL) {
    {NULL, NULL}
};
