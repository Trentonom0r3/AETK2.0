#pragma once

#include <AE_Effect.h>
#include <source_location>
#include <stdexcept>
#include <string>

namespace aetk::core {

class exception : public std::runtime_error {
public:
    exception(PF_Err code, const std::string& message,
        const std::source_location& loc = std::source_location::current())
        : std::runtime_error(message)
        , m_code(code)
        , m_location(loc) {
        m_full_msg = message + " [" + err_to_string(code) + "]\n"
            + "  File:     " + loc.file_name() + ":" + std::to_string(loc.line()) + "\n"
            + "  Function: " + loc.function_name();
    }

    virtual const char* what() const noexcept override {
        return m_full_msg.c_str();
    }

    PF_Err code() const noexcept {
        return m_code;
    }
    const std::source_location& location() const noexcept {
        return m_location;
    }

private:
    PF_Err m_code;
    std::source_location m_location;
    std::string m_full_msg;

public:
    // Defined inline directly inside the class block to resolve linkage promises
    static std::string err_to_string(PF_Err err) {
        switch (err) {
        case PF_Err_NONE:
            return "PF_Err_NONE";
        case PF_Err_OUT_OF_MEMORY:
            return "PF_Err_OUT_OF_MEMORY";
        case PF_Err_INTERNAL_STRUCT_DAMAGED:
            return "PF_Err_INTERNAL_STRUCT_DAMAGED";
        case PF_Err_INVALID_INDEX:
            return "PF_Err_INVALID_INDEX";
        case PF_Err_UNRECOGNIZED_PARAM_TYPE:
            return "PF_Err_UNRECOGNIZED_PARAM_TYPE";
        case PF_Err_INVALID_CALLBACK:
            return "PF_Err_INVALID_CALLBACK";
        case PF_Err_BAD_CALLBACK_PARAM:
            return "PF_Err_BAD_CALLBACK_PARAM";
        case PF_Interrupt_CANCEL:
            return "PF_Interrupt_CANCEL";
        case PF_Err_CANNOT_PARSE_KEYFRAME_TEXT:
            return "PF_Err_CANNOT_PARSE_KEYFRAME_TEXT";
        case 'S!Fd':
            return "kSPSuiteNotFoundError ('S!Fd')";
        case 'stop':
            return "kSPUserCanceledError / kASUserCanceledError ('stop')";
        case 'intr':
            return "kSPOperationInterrupted ('intr')";
        case '!Acq':
            return "kSPCantAcquirePluginError ('!Acq')";
        case '!Rel':
            return "kSPCantReleasePluginError ('!Rel')";
        case 'AlRl':
            return "kSPPluginAlreadyReleasedError ('AlRl')";
        case 'AdEx':
            return "kSPAdapterAlreadyExistsError ('AdEx')";
        case 'BdAL':
            return "kSPBadAdapterListIteratorError ('BdAL')";
        case 'Parm':
            return "kSPBadParameterError ('Parm')";
        case '!Now':
            return "kSPCantChangeBlockDebugNowError ('!Now')";
        case '!Nbl':
            return "kSPBlockDebugNotEnabledError ('!Nbl')";
        case (int32_t)0xFFFFFF6C: // -108
            return "kSPOutOfMemoryError (-108)";
        case 'BkRg':
            return "kSPBlockSizeOutOfRangeError ('BkRg')";
        case 'pFls':
            return "kSPPluginCachesFlushResponse ('pFls')";
        case 'TAdd':
            return "kSPTroubleAddingFilesError ('TAdd')";
        case 'BFIt':
            return "kSPBadFileListIteratorError ('BFIt')";
        case 'TIni':
            return "kSPTroubleInitializingError ('TIni')";
        case 'H!St':
            return "kHostCanceledStartupPluginsError ('H!St')";
        case 'NSPP':
            return "kSPNotASweetPeaPluginError ('NSPP')";
        case 'AISC':
            return "kSPAlreadyInSPCallerError ('AISC')";
        case '?Adp':
            return "kSPUnknownAdapterError ('?Adp')";
        case 'PiLI':
            return "kSPBadPluginListIteratorError ('PiLI')";
        case 'PiH0':
            return "kSPBadPluginHost ('PiH0')";
        case 'AdHo':
            return "kSPCantAddHostPluginError ('AdHo')";
        case 'P!Fd':
            return "kSPPluginNotFound ('P!Fd')";
        case 'CPPL':
            return "kSPCorruptPiPLError ('CPPL')";
        case 'BPrI':
            return "kSPBadPropertyListIteratorError ('BPrI')";
        case 'SExi':
            return "kSPSuiteAlreadyExistsError ('SExi')";
        case 'SRel':
            return "kSPSuiteAlreadyReleasedError ('SRel')";
        case 'SLIt':
            return "kSPBadSuiteListIteratorError ('SLIt')";
        case 'SIVs':
            return "kSPBadSuiteInternalVersionError ('SIVs')";
        // AEGP / A_Err Error Codes (non-overlapping values)
        case 1:
            return "A_Err_GENERIC";
        case 2:
            return "A_Err_STRUCT";
        case 3:
            return "A_Err_PARAMETER";
        case 5:
            return "A_Err_WRONG_THREAD";
        case 6:
            return "A_Err_CONST_PROJECT_MODIFICATION";
        case 13:
            return "A_Err_MISSING_SUITE";
        case 22:
            return "A_Err_NOT_IN_CACHE_OR_COMPUTE_PENDING";
        case 23:
            return "A_Err_PROJECT_LOAD_FATAL";
        case 24:
            return "A_Err_EFFECT_APPLY_FATAL";
        default:
            return "Unknown Error (" + std::to_string(err) + ")";
        }
    }
};

/**
 * @brief Helper to check a PF_Err and throw with precise call-site
 * localization.
 */
inline void check_err(PF_Err err, const std::string& message = "After Effects SDK Error",
    const std::source_location loc = std::source_location::current()) {
    if (err != PF_Err_NONE) {
        throw exception(err, message, loc);
    }
}

} // namespace aetk::core