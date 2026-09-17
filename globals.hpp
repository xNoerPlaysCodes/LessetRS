#ifndef LESSET
    #include "lesset.hpp"
#endif

namespace globals
{
    inline std::ostringstream oss;
    inline lessetB::Options options;

    inline std::unordered_map<std::string,std::string> userVariables;

    inline std::pair<std::vector<double>,std::vector<double>> points; 

    inline std::string ans;
    inline std::string errorMessage;
    inline bool error{};

    inline bool passedCalculationsFile{};
    inline bool passedInAsArg{};

    inline bool useDecimalComma{};

    inline int decimalPrecision{lessetB::defaultPrecision};

    inline bool debugCout{};
    inline bool debugCoutUsed{};

    inline std::unordered_map<std::string, std::vector<lessetB::Token>> tokenMemory;
    
    const std::unordered_map<std::string, lessetB::token_t> symbols
    {
        {"+",    lessetB::token_t::BINARYOP},
        {"*",    lessetB::token_t::BINARYOP},
        {"/",    lessetB::token_t::BINARYOP},
        {"//",    lessetB::token_t::BINARYOP},
        {":",    lessetB::token_t::BINARYOP},
        {"^",    lessetB::token_t::BINARYOP},
        {"%",    lessetB::token_t::BINARYOP},
        {"<",    lessetB::token_t::BINARYOP},
        {">",    lessetB::token_t::BINARYOP},
        {"=",    lessetB::token_t::BINARYOP},
        {"mod",    lessetB::token_t::BINARYOP},
        {"fmod",    lessetB::token_t::BINARYOP},
        {"rmod",    lessetB::token_t::BINARYOP},
        {"nPk",    lessetB::token_t::BINARYOP},
        {"nCk",    lessetB::token_t::BINARYOP},
        {"**",    lessetB::token_t::BINARYOP},
        {"AND",    lessetB::token_t::BINARYOP},
        {"XOR",    lessetB::token_t::BINARYOP},
        {"AROUND",    lessetB::token_t::BINARYOP},
        {"NOR",    lessetB::token_t::BINARYOP},
        {"OR",    lessetB::token_t::BINARYOP},
        {"=!",    lessetB::token_t::BINARYOP},
        {">=",    lessetB::token_t::BINARYOP},
        {"<=",    lessetB::token_t::BINARYOP},

        {"!", lessetB::token_t::UNARYOP},
        {"-", lessetB::token_t::UNARYOP},
        {"!!", lessetB::token_t::UNARYOP},

        {"pi", lessetB::token_t::CONSTANT },
        {"i", lessetB::token_t::CONSTANT },
        {"e", lessetB::token_t::CONSTANT },
        {"a", lessetB::token_t::CONSTANT },
        {"rnd", lessetB::token_t::CONSTANT },
        {"rndint", lessetB::token_t::CONSTANT },
        {"ec", lessetB::token_t::CONSTANT },
        {"c", lessetB::token_t::CONSTANT },
        {"R", lessetB::token_t::CONSTANT },
        {"G", lessetB::token_t::CONSTANT },
        {"g", lessetB::token_t::CONSTANT },
        {"o", lessetB::token_t::CONSTANT },
        {"h", lessetB::token_t::CONSTANT },
        {"k", lessetB::token_t::CONSTANT },
        {"H0", lessetB::token_t::CONSTANT },
        {"Z0", lessetB::token_t::CONSTANT },
        {"U0", lessetB::token_t::CONSTANT },
        {"E0", lessetB::token_t::CONSTANT },
        {"tau", lessetB::token_t::CONSTANT },
        {"phi", lessetB::token_t::CONSTANT },
        {"eul", lessetB::token_t::CONSTANT },
        {"rad", lessetB::token_t::CONSTANT },
        {"dgr", lessetB::token_t::CONSTANT },
        {"inf", lessetB::token_t::CONSTANT },
        {"ppm", lessetB::token_t::CONSTANT },
        {"ppb", lessetB::token_t::CONSTANT },
        {"ppt", lessetB::token_t::CONSTANT },
        {"prc", lessetB::token_t::CONSTANT },
        {"me", lessetB::token_t::CONSTANT },
        {"ma", lessetB::token_t::CONSTANT },
        {"Na", lessetB::token_t::CONSTANT },
        {"true", lessetB::token_t::CONSTANT },
        {"false", lessetB::token_t::CONSTANT },
        {"ans", lessetB::token_t::CONSTANT},

        {"real", lessetB::token_t::FUNCTION},
        {"imag", lessetB::token_t::FUNCTION},
        {"arg", lessetB::token_t::FUNCTION},
        {"norm",lessetB::token_t::FUNCTION},
        {"conj",lessetB::token_t::FUNCTION},
        {"proj",lessetB::token_t::FUNCTION},
        // {"polar",lessetB::token_t::FUNCTION},
        {"x",lessetB::token_t::VARIABLE},

        {"sinc", lessetB::token_t::FUNCTION},
        {"sinc^2", lessetB::token_t::FUNCTION},
        {"exp", lessetB::token_t::FUNCTION},
        {"sign", lessetB::token_t::FUNCTION},
        {"sqrt", lessetB::token_t::FUNCTION},
        {"cbrt", lessetB::token_t::FUNCTION},
        {"qtrt", lessetB::token_t::FUNCTION},

        {"sin", lessetB::token_t::FUNCTION},
        {"cos", lessetB::token_t::FUNCTION},
        {"cis", lessetB::token_t::FUNCTION},
        {"cas", lessetB::token_t::FUNCTION},
        {"tan", lessetB::token_t::FUNCTION},
        {"sinh", lessetB::token_t::FUNCTION},
        {"cosh", lessetB::token_t::FUNCTION},
        {"tanh", lessetB::token_t::FUNCTION},

        {"asin", lessetB::token_t::FUNCTION},
        {"acos", lessetB::token_t::FUNCTION},
        {"atan", lessetB::token_t::FUNCTION},
        {"asinh", lessetB::token_t::FUNCTION},
        {"acosh", lessetB::token_t::FUNCTION},
        {"atanh", lessetB::token_t::FUNCTION},

        {"sec", lessetB::token_t::FUNCTION},
        {"csc", lessetB::token_t::FUNCTION},
        {"cot", lessetB::token_t::FUNCTION},
        {"sech", lessetB::token_t::FUNCTION},
        {"csch", lessetB::token_t::FUNCTION},
        {"coth", lessetB::token_t::FUNCTION},

        {"asec", lessetB::token_t::FUNCTION},
        {"acsc", lessetB::token_t::FUNCTION},
        {"acot", lessetB::token_t::FUNCTION},
        {"asech", lessetB::token_t::FUNCTION},
        {"acsch", lessetB::token_t::FUNCTION}, //aschhschhshuhuschush
        {"acoth", lessetB::token_t::FUNCTION},
        {"prime", lessetB::token_t::FUNCTION},

        {"ln", lessetB::token_t::FUNCTION},
        {"ln^2", lessetB::token_t::FUNCTION},
        {"abs", lessetB::token_t::FUNCTION},
        {"floor", lessetB::token_t::FUNCTION},
        {"trunc", lessetB::token_t::FUNCTION},
        {"ceil", lessetB::token_t::FUNCTION},
        {"bround", lessetB::token_t::FUNCTION},
        {"round", lessetB::token_t::FUNCTION},
        {"sat", lessetB::token_t::FUNCTION},
        {"ReLU", lessetB::token_t::FUNCTION},
        {"sstep", lessetB::token_t::FUNCTION},
        {"lgam", lessetB::token_t::FUNCTION},
        {"gam", lessetB::token_t::FUNCTION},

        {"sin^2", lessetB::token_t::FUNCTION},
        {"cos^2", lessetB::token_t::FUNCTION},
        {"cis^2", lessetB::token_t::FUNCTION},
        {"cas^2", lessetB::token_t::FUNCTION},
        {"tan^2", lessetB::token_t::FUNCTION},
        {"sinh^2", lessetB::token_t::FUNCTION},
        {"cosh^2", lessetB::token_t::FUNCTION},
        {"tanh^2", lessetB::token_t::FUNCTION},

        {"asin^2", lessetB::token_t::FUNCTION},
        {"acos^2", lessetB::token_t::FUNCTION},
        {"atan^2", lessetB::token_t::FUNCTION},
        {"asinh^2", lessetB::token_t::FUNCTION},
        {"acosh^2", lessetB::token_t::FUNCTION},
        {"atanh^2", lessetB::token_t::FUNCTION},

        {"sec^2", lessetB::token_t::FUNCTION},
        {"csc^2", lessetB::token_t::FUNCTION},
        {"cot^2", lessetB::token_t::FUNCTION},
        {"sech^2", lessetB::token_t::FUNCTION},
        {"csch^2", lessetB::token_t::FUNCTION},
        {"coth^2", lessetB::token_t::FUNCTION},

        {"asec^2", lessetB::token_t::FUNCTION},
        {"acsc^2", lessetB::token_t::FUNCTION},
        {"acot^2", lessetB::token_t::FUNCTION},
        {"asech^2", lessetB::token_t::FUNCTION},
        {"acsch^2", lessetB::token_t::FUNCTION}, //aschhschhshuhuschush^2
        {"acoth^2", lessetB::token_t::FUNCTION},

    };

    const std::unordered_map<std::string, lessetB::token_t> multiArgFunctions
    {
        {"root", lessetB::token_t::ROOT},
        {"log", lessetB::token_t::LOG},
        {"diff", lessetB::token_t::DIFF},
        {"mean", lessetB::token_t::MEAN},
        {"median", lessetB::token_t::MEDIAN},
        {"stdevp", lessetB::token_t::STDEVP},
        {"gcf", lessetB::token_t::GCF},
        {"gcd", lessetB::token_t::GCF},
        {"hcf", lessetB::token_t::GCF},
        {"hcd", lessetB::token_t::GCF},
        {"lcm", lessetB::token_t::LCM},
        {"rndint", lessetB::token_t::RNDINT},
        {"rndsel", lessetB::token_t::RNDSEL},
        {"max", lessetB::token_t::MAX},
        {"atan2", lessetB::token_t::ATAN2},
        // {"smax", lessetB::token_t::SMAX},
        {"min", lessetB::token_t::MIN},
        // {"smin", lessetB::token_t::SMIN},
        {"sabs", lessetB::token_t::SABS},
        {"mix", lessetB::token_t::MIX},
        {"if", lessetB::token_t::IF},
        {"round", lessetB::token_t::ROUND},
        {"trunc", lessetB::token_t::TRUNC},
    };

    inline std::mutex accessParamsMtx;

    inline std::unordered_map<std::string, lessetB::Function> userFunctions
    {

    };

    inline std::unordered_map<std::string, lessetB::Function> prevUserFunctions
    {

    };
    inline size_t functionCallID{};
    inline std::vector<std::unordered_map<std::string,std::string>> fnCallParams; // Stores arguments for function calls.

    const std::unordered_map<std::string, std::string> constants
    {
        {"e" , "2.718281828459045235360287471352662497757247093699959574966967627724076630353547594571382178525166427427466391932003059921817413596629043572900334295260595630738132328627943490763233829880753195251019011573834187930702154089149934884167509244761460668"},
        {"pi" , "3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148086513282306647093844609550582231725359408128481117450284102701938521105559644622948954930381964428810975665933446128475648233786783165271201909"},
        {"tau" , "6.283185307179586476925286766559005768394338798750211641949889184615632812572417997256069650684234135964296173026564613294187689219101164463450718816256962234900568205403877042211119289245897909860763928857621951331866892256951296467573566330542403818"},
        {"phi" , "1.618033988749894848204586834365638117720309179805762862135448622705260462818902449707207204189391137484754088075386891752126633862223536931793180060766726354433389086595939582905638322661319928290267880675208766892501711696207032221043216269548626297"},
        {"eul" , "0.57721566490153286060651209008240243104215933593992359880576723488486772677766467093694706329174674951463144724980708248096050401448654283622417399764492353625350033374293733773767394279259525824709491600873520394816567085323315177661152862119950150798"},
        {"rad" , "57.29577951308232087679815481410517033240547246656432154916024386120284714832155263244096899585111094418622338163286489328144826460124831503606826786341194212252638809746726792630798870289311076793826144263826315820961046048702050644425965684112017192"},
        {"dgr" , "0.01745329251994329576923690768488612713442871888541725456097191440171009114603449443682241569634509482212304492507379059248385469227528101239847421893404711731916824501501076956169755358123860530516878869127117208703296358960264249018770435091817334394"},
        {"ppm" , "0.000001"},
        {"ppb" , "0.000000001"},
        {"ppt" , "0.000000000001"},
        {"prc" , "0.01"},
        {"c" , "299792458"},
        {"G" , "6.6743e-11"},
        {"g" , "9.80665"},
        {"o" , "5.670374419e-08"},
        {"k" , "1.380649e-23"},
        {"a" , "0.0072973525693"},
        {"h" , "6.62607015e-34"},
        {"i" , "(0,1)"}, // :o
        {"inf" , "inf"},
        {"true" , "1"},
        {"false" , "0"},
        {"H0" , "2.2e-18"},
        {"me" , "5.9722e+24"},
        {"ec" , "1.602176634e-19"},
        {"Z0" , "376.730313668"},
        {"U0" , "1.25663706212e-06"},
        {"E0" , "8.8541878128e-12"},
        {"ma" , "1.6605390666e-27"},
        {"R" , "8.31446261815"},
        {"Na" , "6.02214076e+23"},
        {"ans" , "ans"},
        {"rnd", "rnd"}, // These are replaced later
        {"rndint","rndint"},
    };

    const std::unordered_map<std::string, std::string> valueToConstant
    {
        { "2.718281828459045235360287471352662497757247093699959574966967627724076630353547594571382178525166427427466391932003059921817413596629043572900334295260595630738132328627943490763233829880753195251019011573834187930702154089149934884167509244761460668","ℯ"},
        { "3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148086513282306647093844609550582231725359408128481117450284102701938521105559644622948954930381964428810975665933446128475648233786783165271201909","π"},
        { "6.283185307179586476925286766559005768394338798750211641949889184615632812572417997256069650684234135964296173026564613294187689219101164463450718816256962234900568205403877042211119289245897909860763928857621951331866892256951296467573566330542403818","τ"},
        { "1.618033988749894848204586834365638117720309179805762862135448622705260462818902449707207204189391137484754088075386891752126633862223536931793180060766726354433389086595939582905638322661319928290267880675208766892501711696207032221043216269548626297","φ"},
        { "0.57721566490153286060651209008240243104215933593992359880576723488486772677766467093694706329174674951463144724980708248096050401448654283622417399764492353625350033374293733773767394279259525824709491600873520394816567085323315177661152862119950150798","γ"},
        { "57.29577951308232087679815481410517033240547246656432154916024386120284714832155263244096899585111094418622338163286489328144826460124831503606826786341194212252638809746726792630798870289311076793826144263826315820961046048702050644425965684112017192","rad"},
        { "0.01745329251994329576923690768488612713442871888541725456097191440171009114603449443682241569634509482212304492507379059248385469227528101239847421893404711731916824501501076956169755358123860530516878869127117208703296358960264249018770435091817334394","dgr" },
        { "299792458","c"},
        { "6.6743e-11","G"},
        { "9.80665","g"},
        {"5.670374419e-08","o" },
        {"1.380649e-23","k" },
        {"0.0072973525693","a" },
        {"6.62607015e-34","h" },
        {"inf","∞" },
        {"-inf","-∞" },
        {"2.2e-18","H0" },
        {"5.9722e+24","me" },
        {"1.602176634e-19","ec" },
        {"376.730313668","Z0" },
        {"1.25663706212e-06","U0" },
        {"8.8541878128e-12","E0" },
        {"1.6605390666e-27","ma" },
        {"8.31446261815","R" },
        {"6.02214076e+23","Na" },
        {"(0,1)","i" },
        {"Not a Number","Not a Number"},
    };

    const std::unordered_map<std::string, size_t> opToID
    {
        {"+",    0},
        {"*",    1},
        {"/",    2},
        {":",    2},
        {"h*",    3},
        {"^",    4},
        {"%",    5},
        {"<",    6},
        {">",    7},
        {"=",    8},
        {"mod",    5},
        {"fmod",    10},
        {"rmod",    11},
        {"nPk",    12},
        {"nCk",    13},
        {"**",    4},
        {"AND",    15},
        {"XOR",    16},
        {"AROUND",    17},
        {"NOR",    18},
        {"OR",    19},
        {"=!",    20},
        {">=",    21},
        {"<=",    22},

        {"!", 23},
        {"-", 24},
        {"!!", 25},

        {"sinc", 26},
        {"sinc^2", 27},
        {"exp", 28},
        {"sign", 29},
        {"sqrt", 30},
        {"cbrt", 31},
        {"qtrt", 32},

        {"sin", 33},
        {"cos", 34},
        {"tan", 35},
        {"sinh", 36},
        {"cosh", 37},
        {"tanh", 38},

        {"asin", 39},
        {"acos", 40},
        {"atan", 41},
        {"asinh", 42},
        {"acosh", 43},
        {"atanh", 44},

        {"sec", 45},
        {"csc", 46},
        {"cot", 47},
        {"sech", 48},
        {"csch", 49},
        {"coth", 50},

        {"asec", 51},
        {"acsc", 52},
        {"acot", 53},
        {"asech", 54},
        {"acsch", 55}, //aschhschhshuhuschush
        {"acoth", 56},
        {"prime", 57},

        {"ln", 58},
        {"ln^2", 59},
        {"abs", 60},
        {"floor", 61},
        {"trunc", 62},
        {"ceil", 63},
        {"bround", 64},
        {"round", 65},
        {"sat", 66},
        {"ReLU", 67},
        {"sstep", 68},
        {"lgam", 69},
        {"gam", 70},

        {"sin^2", 71},
        {"cos^2", 72},
        {"tan^2", 73},
        {"sinh^2", 74},
        {"cosh^2", 75},
        {"tanh^2", 76},

        {"asin^2", 77},
        {"acos^2", 78},
        {"atan^2", 79},
        {"asinh^2", 80},
        {"acosh^2", 81},
        {"atanh^2", 82},

        {"sec^2", 83},
        {"csc^2", 84},
        {"cot^2", 85},
        {"sech^2", 86},
        {"csch^2", 87},
        {"coth^2", 88},

        {"asec^2", 89},
        {"acsc^2", 90},
        {"acot^2", 91},
        {"asech^2", 92},
        {"acsch^2", 93}, //aschhschhshuhuschush^2
        {"acoth^2", 94},
    
        {"real", 95}, //aschhschhshuhuschush^2
        {"imag", 96},    
        {"arg", 97},  
        {"norm",98},
        {"conj",99},
        {"proj",100},
        {"cis",101},
        {"cis^2",102},
        {"cas",103},
        {"cas^2",104},
        {"//",105},
        // {"polar",101}  
    };

    const std::unordered_map<std::string, lessetB::pass> opToPriority
    {
        {"+",    lessetB::ADDITION},
        {"*",    lessetB::MULTIPLICATION},
        {"//",    lessetB::MULTIPLICATION},
        {"/",    lessetB::MULTIPLICATION},
        {":",    lessetB::MULTIPLICATION},
        {"h*",    lessetB::MULTIPLICATIONIMPLICIT},
        {"^",    lessetB::EXPONENTIATION},
        {"%",    lessetB::MULTIPLICATION},
        {"<",    lessetB::COMPARISONS},
        {">",    lessetB::COMPARISONS},
        {"=",    lessetB::COMPARISONS},
        {"mod",    lessetB::MULTIPLICATION},
        {"fmod",    lessetB::MULTIPLICATION},
        {"rmod",    lessetB::MULTIPLICATION},
        {"nPk",    lessetB::MULTIPLICATION},
        {"nCk",    lessetB::MULTIPLICATION},
        {"**",    lessetB::EXPONENTIATION},
        {"AND",    lessetB::LOGICALS},
        {"XOR",    lessetB::LOGICALS},
        {"AROUND",    lessetB::LOGICALS},
        {"NOR",    lessetB::LOGICALS},
        {"OR",    lessetB::LOGICALS},
        {"=!",    lessetB::COMPARISONS},
        {">=",    lessetB::COMPARISONS},
        {"<=",    lessetB::COMPARISONS},
    };
}; // Namespace globals