
#ifdef LESSET
    #error "Only include lesset.hpp once."
#endif

#define LESSET

#include <complex>
#include <limits>
#include <cctype>
#include <random>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <boost/math/constants/constants.hpp>
#include <sstream>
#include <boost/multiprecision/cpp_complex.hpp>
#include <boost/math/ccmath/fmod.hpp>
#include <type_traits>
#include <unordered_map>
#include <future>

namespace lessetB
{
    
bool isValidInput(char);

using namespace boost::multiprecision;

inline std::random_device randev;
inline std::mt19937 randomMt(randev());

constexpr int maxPrecision = 250;     // Really powerful, upper limit for precision in calculations. Must be >=16 I think. Constants have up to 250 decimal places.
constexpr int defaultPrecision = 100; // What is shown by default. Should not be higher than max.

static_assert(maxPrecision>=defaultPrecision, "Default precision should not exceed maximum");

#define MAX_KEYWORD_LENGTH 15 // Also limits custom keywords

enum pass
{
    SUBEXPRESSIONS,
    FUNCTIONS,
    UNARYOPS,
    EXPONENTIATION,
    UNARYMINUS,
    MULTIPLICATIONIMPLICIT,
    MULTIPLICATION,
    ADDITION,
    COMPARISONS,
    LOGICALS
};

enum class token_t
{
    BINARYOP,
    UNARYOP,
    VARIABLE,
    CONSTANT,
    SUMVAR,
    FUNCTION, // Simple baby functions
    
    SUBEXPR,
    // Multiarg functions
    IF,
    NUMBER,
    ROOT,
    LOG,
    DIFF,
    MEAN,
    MEDIAN,
    STDEVP,
    GCF,
    LCM,
    RNDINT,
    ROUND,
    TRUNC,
    RNDSEL,
    ABS,
    MAX,
    SMAX,
    SMIN,
    SABS,
    MIX,
    MIN,
    ATAN2,

    // Custom functions take one argument but act like multiargs
    CUSTOMFN,

    INVALID
};

enum class tokenCategory_t
{
    NUMBER,
    FUNCTION,
    SUBEXPR,
    OPERATOR,
    INVALID
};

bool isNumberPart(char input);

inline bool isRealNumber(const std::string &input, bool disallowSpecials=false);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Point
{
    const float x{};
    const float y{};
    Point(float inX, float inY) : x(inX), y(inY){}
};

struct Frac
{
    float numer{};
    float denom{};
    Frac(float inX, float inY) : numer(inX), denom(inY){}
};

struct Options
{
    bool graph{};
    cpp_complex<maxPrecision> xMin{};
    cpp_complex<maxPrecision> xMax{};  
    cpp_complex<maxPrecision> xStep{}; // Hey, reference
    cpp_complex<maxPrecision> aroundTruthinessLeniency{0.01};
    bool interpolateDiscontinuities{};
    bool prioritizeImplicitMultiplication{true};
    std::string definesFunction=""; // Used to prevent defining a custom function using itself
    bool prettyPrinting{true};
    std::string ans;
    
};

struct Variable
{
    Variable(std::string inName, std::string inValue) : name(inName), value(inValue){}
    std::string name;
    std::string value;
};

struct Function
{
    std::string definition;
    std::vector<std::string> argNames;
    size_t argc{1};
};

bool evaluateEquation(Options &options, bool passedInAsArg,bool passedCalculationsFile, std::string &equation, std::string &resultHistory, std::string &result, std::unordered_map<std::string,std::string> &userVariables, std::unordered_map<std::string,Function> &userFunctions);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


inline bool sortByReal(std::complex<double> a, std::complex<double> b)
{
    return a.real()<b.real();
}

inline bool sortByReal(cpp_complex<maxPrecision> a, cpp_complex<maxPrecision> b)
{
    return a.real()<b.real();
}



class Token;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "globals.hpp"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Token
{

    private:

    token_t tokenType{};
    tokenCategory_t tokenCategory{};
    std::string tokenValue{};
    std::complex<double> tokenNumber{NAN,NAN};

    ///////////////////////////////////////////////
    const token_t determineType(std::string &value)
    {

        if(isRealNumber(value)) return token_t::NUMBER;

        for(size_t i{MAX_KEYWORD_LENGTH}; i>0; i--)
        {
            if(globals::userFunctions.find(value.substr(0,i))!=globals::userFunctions.end())
            {
                return token_t::CUSTOMFN;
            }
        }

        if(value.at(0)=='(')
        {
            std::stringstream ss;
            ss<<value;
            bool isComplexNum{true};
            std::complex<float> num(NAN,NAN);
            ss<<')';
            ss>>num;

            if(num.real()!=num.real() && num.imag() != num.imag())
            {
                isComplexNum=false;
            }
            if(isComplexNum) return token_t::CONSTANT;
        }
        
        if((globals::userVariables.find(value)!=globals::userVariables.end()) || globals::constants.find(value)!=globals::constants.end()) return token_t::CONSTANT;

        if(value=="h*") return token_t::BINARYOP;

        else if(isSubexpr(value)) return token_t::SUBEXPR;
        else if(isAbs(value)) return token_t::ABS;
        token_t tokenTypeCandidate = isMultiArgFunction(value);

        if(tokenTypeCandidate==token_t::INVALID)
        {
            std::string candidate;
            for(size_t i{MAX_KEYWORD_LENGTH}; i>0; i--)
            {
                candidate=value.substr(0,i);
                if(globals::symbols.find(candidate)!=globals::symbols.end())
                {
                    return globals::symbols.find(candidate)->second;
                }
            }
        }
        return tokenTypeCandidate;
    }

    ///////////////////////////////////////////////
    token_t isMultiArgFunction(std::string &input)
    {
        size_t offset{};
        token_t type{token_t::INVALID};

        for(size_t i{MAX_KEYWORD_LENGTH}; i>0; i--)
        {
            if(globals::multiArgFunctions.find(input.substr(0,i))!=globals::multiArgFunctions.end())
            {
                type=globals::multiArgFunctions.find(input.substr(0,i))->second;
                offset=globals::multiArgFunctions.find(input.substr(0,i))->first.length()+1;
                break;
            }
        }
        if(type==token_t::INVALID) return type;
        for(size_t i{offset}; i<input.length(); i++)
        {
            tokenValue.push_back(input.at(i));
        }
        if(offset>=input.length()) return token_t::INVALID;
        return type;
    }
    ///////////////////////////////////////////////
    bool isAbs(std::string &input)
    {
        if((input.at(0)!='|')&&input.find("abs(")!=0) return false;
        
        if(input.at(0)=='|') for(size_t i{1}; i<input.length()-1; i++) tokenValue.push_back(input.at(i));
        else for(size_t i{4}; i<input.length(); i++) tokenValue.push_back(input.at(i));
        if(input.at(input.length()-1)!='|') tokenValue.push_back(input.at(input.length()-1));
        for(size_t i{}; i<input.length(); i++)
        {
            if(input.at(i)!='|') return true;
        }
        return false;
    }    
    ///////////////////////////////////////////////
    static bool isSubexpr(std::string &input)
    {
        if(input.find(')')!=std::string::npos && input.length()<2) return false;
        if(input.at(0)=='(')
        {
            input.erase(0, 1);
            return true;
        }
        return false;
    }
    ///////////////////////////////////////////////
    static std::string replaceConstants(std::string &input)
    {

        if(globals::constants.find(input)!=globals::constants.end()) return globals::constants.find(input)->second;
        if(globals::userVariables.find(input)!=globals::userVariables.end()) return globals::userVariables.find(input)->second;

        return input;
    }
    ///////////////////////////////////////////////
    static tokenCategory_t determineTokenCategory(token_t type) 
    {
        if(type==token_t::NUMBER || type==token_t::VARIABLE || type==token_t::CONSTANT || type==token_t::SUMVAR) return tokenCategory_t::NUMBER;

        if(type==token_t::FUNCTION)                                                                              return tokenCategory_t::FUNCTION;

        if(type==token_t::BINARYOP || type==token_t::UNARYOP)                                                    return tokenCategory_t::OPERATOR;

        if(type==token_t::INVALID)                                                                               return tokenCategory_t::INVALID;
        
        return tokenCategory_t::SUBEXPR;
    }
    ///////////////////////////////////////////////
    ///////////////////////////////////////////////

    public:
    Token(const std::pair<std::string,std::string> var)
    {
        tokenType=token_t::CONSTANT;
        tokenCategory=tokenCategory_t::NUMBER;
        tokenValue=var.second;
        if(var.second.find("inf")==std::string::npos && var.second.find("nan")==std::string::npos) tokenNumber=boost::lexical_cast<std::complex<double>>(var.second);
    }
    Token(std::string value)
    {
        
        if(globals::options.graph) // The things you do to make graphing faster... this block basically shortcuts the regular procedure for making a token in case it's a number, the common case.
        {
            if(value=="(0,1")
            {
                tokenNumber=std::complex<double>(0,1);
                tokenType=token_t::CONSTANT;
                tokenCategory=tokenCategory_t::NUMBER;
                return;
            }
            bool isComplexNum{true};
            token_t potentialNumberType{token_t::INVALID};
            if(isRealNumber(value,true))
            {
                
                tokenType=token_t::NUMBER;
                tokenCategory=tokenCategory_t::NUMBER;
                tokenNumber=boost::lexical_cast<std::complex<double>>(value);
                return;
            }
            else if(value.at(0)=='(')
            {
                std::stringstream ss;
                ss<<value;
                
                std::complex<float> num(NAN,NAN);
                ss<<')';
                ss>>num;

                if(num.real()!=num.real() && num.imag() != num.imag())
                {
                    isComplexNum=false;
                }
                if(isComplexNum) potentialNumberType=token_t::NUMBER;
            }
            if(globals::userVariables.find(value)!=globals::userVariables.end() || globals::constants.find(value)!=globals::constants.end())
            {
                potentialNumberType=token_t::CONSTANT;
                tokenValue=replaceConstants(value);
                if(tokenValue=="rnd" || tokenValue=="rndint") potentialNumberType=token_t::INVALID;
            }
            if(potentialNumberType!=token_t::INVALID && potentialNumberType!=token_t::CONSTANT) 
            {
                if(isComplexNum) potentialNumberType=token_t::CONSTANT;
                if(tokenValue.find("inf")!=std::string::npos)
                {
                    tokenNumber.real(INFINITY);
                    tokenNumber.imag(0); 
                    tokenType=potentialNumberType;
                    tokenCategory=tokenCategory_t::NUMBER;
                    return;
                }
                else if(tokenValue.find(',')!=std::string::npos) tokenNumber=boost::lexical_cast<std::complex<double>>(tokenValue+")");
                else if(tokenValue.find('(')==std::string::npos) tokenNumber=boost::lexical_cast<std::complex<double>>(tokenValue);
                else tokenNumber=boost::lexical_cast<std::complex<double>>(tokenValue+",0)");

                if(tokenNumber==tokenNumber || tokenValue=="nan")
                {
                    tokenType=potentialNumberType;
                    tokenCategory=tokenCategory_t::NUMBER;
                    return;
                }
                
            }
            else if (potentialNumberType==token_t::CONSTANT)
            {
                if(tokenValue.find("inf")!=std::string::npos)
                {
                    tokenNumber.real(INFINITY);
                    tokenNumber.imag(0); 
                    tokenType=potentialNumberType;
                    tokenCategory=tokenCategory_t::NUMBER;
                    return;
                }
                
                if(tokenValue.find('n')==std::string::npos)
                {
                    if(tokenValue.find(',')!=std::string::npos) tokenNumber=boost::lexical_cast<std::complex<double>>(tokenValue);
                    else tokenNumber=boost::lexical_cast<std::complex<double>>("("+tokenValue+",0)");
                }
                tokenType=token_t::CONSTANT;
                tokenCategory=tokenCategory_t::NUMBER;
                return;
            }
        }

        tokenType = determineType(value);
        if(tokenType==token_t::CONSTANT) tokenValue=replaceConstants(value);
        
        tokenCategory=determineTokenCategory(tokenType);
        if(tokenValue.empty())tokenValue = value;

    }

    Token(cpp_complex<maxPrecision> &value)
    {
        tokenType = token_t::NUMBER;        
        tokenCategory=tokenCategory_t::NUMBER;
        tokenValue=value.str(maxPrecision);
    }


    Token(std::complex<double> value)
    {
        tokenType = token_t::NUMBER;        
        tokenCategory=tokenCategory_t::NUMBER;
        tokenNumber=value;
    }
    ///////////////////////////////////////////////
    template<typename T>
    T number(T xValue=NAN) const
    {
        if constexpr((std::is_same_v<T,cpp_complex<maxPrecision>> || std::is_same_v<T,std::complex<double>>))
        {
            if(xValue.real()!=NAN && this->tokenType==token_t::VARIABLE)
            {
                return xValue;
            }

            if(tokenValue=="rnd" || tokenValue=="rndint") return NAN;

            if (tokenType != token_t::NUMBER && tokenType != token_t::CONSTANT) return NAN;
            if constexpr(std::is_same<T,cpp_complex<maxPrecision>>()) return static_cast<cpp_complex<maxPrecision>>(tokenValue);
            else if constexpr(std::is_same<T,std::complex<double>>()) return tokenNumber;
        }
        else return T();
    }
    ///////////////////////////////////////////////
    std::string value() const
    {
        if(!tokenValue.empty()) return tokenValue;
        else
        {
            std::ostringstream oss;
            oss.precision(std::numeric_limits<double>::digits10);
            oss<<tokenNumber;
            return oss.str();
        }
    }  
    token_t type() const
    {
        return tokenType;
    }
    tokenCategory_t category() const
    {
        return tokenCategory;
    }
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<Token> getTokens(const std::string&, bool resetFirstRun=false, bool noMemoize=false, const std::unordered_map<std::string, std::string> &fnCallParams = std::unordered_map<std::string, std::string>());
void parseMultiArgFunction(const std::string &input, std::vector<Token> &tokens, std::string functionName, size_t &i, bool &inFunctionCall, size_t argCount=1);
inline Function getFnFromSignature(const std::string &sig);
inline Function getFnFromArgs(const std::string &def, std::string &sig);

template<typename T=cpp_complex<maxPrecision>>
T calculation(std::vector<Token>, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams = std::unordered_map<std::string, std::string>());

std::pair<std::vector<double>,std::vector<double>> calculationCallerGraphing(std::vector<Token> &tokens, double xValue, double xValueMax, size_t threadNumber);
inline std::string calculationCallerTable(std::vector<Token> &tokens, cpp_complex<maxPrecision> xValue, cpp_complex<maxPrecision> xValueMax, size_t threadNumber, size_t totalCalculations);

template<typename T=cpp_complex<maxPrecision>>
T evaluateUnary(Token&, Token&, const T xValue);

template<typename T=cpp_complex<maxPrecision>>
T evaluateBinary(Token&, Token&, Token&, const T xValue);

template<typename T=cpp_complex<maxPrecision>>
bool evaluateArgs( const Token &arg, const T xValue, std::vector<T>&argVals, const std::unordered_map<std::string, std::string> &fnCallParams, size_t argsToEval=SIZE_MAX);

inline bool containsX(const std::string &equation);

Frac decimalToFraction(cpp_complex<maxPrecision> enumerator, size_t precision = 15);

template <typename T = cpp_complex<maxPrecision>> T evaluateAbs(const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams);
template <typename T = cpp_complex<maxPrecision>> T evaluateIf(const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams);
template <typename T = cpp_complex<maxPrecision>> T evaluateLog(const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams);
template <typename T = cpp_complex<maxPrecision>> T evaluateRoot(const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams);
template <typename T = cpp_complex<maxPrecision>> T evaluateMean(const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams);
template <typename T = cpp_complex<maxPrecision>> T evaluateMedian(const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams);
template <typename T = cpp_complex<maxPrecision>> T evaluateStdevp(const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams);
template <typename T = cpp_complex<maxPrecision>> T evaluateRndsel(const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams);
template <typename T = cpp_complex<maxPrecision>> T evaluateMax(const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams);
template <typename T = cpp_complex<maxPrecision>> T evaluateLeast(const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams);
template <typename T = cpp_complex<maxPrecision>> T evaluateGcf(const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams);
template <typename T = cpp_complex<maxPrecision>> T evaluateLcm(const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams);
template <typename T = cpp_complex<maxPrecision>> T evaluateRound( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams);
template <typename T = cpp_complex<maxPrecision>> T evaluateFloor( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams);
template <typename T = cpp_complex<maxPrecision>> T evaluateAtan2( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams);
template <typename T = cpp_complex<maxPrecision>> T evaluateCustomFn( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams);

bool addIdentifier(const Variable &newVariable);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline bool evaluateEquation(Options &options, bool passedInAsArg,bool passedCalculationsFile, std::string &equation, std::string &resultHistory, std::string &result, std::unordered_map<std::string,std::string> &userVariables, std::unordered_map<std::string,Function> &userFunctions)
{
    if(globals::debugCout) std::cout<<"evaluateEquation()\n";

    globals::oss.precision(maxPrecision);
    std::string initialEquation=equation;
    globals::error=false;
    globals::userFunctions=userFunctions;
    globals::userVariables=userVariables;
    globals::passedInAsArg=passedInAsArg;
    globals::passedCalculationsFile=passedCalculationsFile;
    globals::options=options;
    std::ostringstream resultAsOSStream;
    resultAsOSStream.precision(maxPrecision);
    globals::ans="nan";
    if(!options.ans.empty()) globals::ans=options.ans; 
    getTokens("",true);

    if(equation.find('#') != std::string::npos) equation.erase(equation.find('#'));

    if(equation.length()==0) return true;
                   

    for(size_t i{}; i<equation.length(); i++) // Replace some unicode
    {
        if(equation.find("≤",i)==i) equation.replace(i,sizeof("≤")-1,"<=");
        else if(equation.find("ᵉ",i)==i) equation.replace(i,sizeof("ᵉ")-1,"ec");
        else if(equation.find("α",i)==i) equation.replace(i,sizeof("α")-1,"a");

        else if(equation.find("τ",i)==i) equation.replace(i,sizeof("τ")-1,"tau");
        else if(equation.find("ξ",i)==i) equation.replace(i,sizeof("ξ")-1,"rnd");
        else if(equation.find("∞",i)==i) equation.replace(i,sizeof("∞")-1,"inf");
        else if(equation.find("φ",i)==i) equation.replace(i,sizeof("φ")-1,"phi");
        else if(equation.find("√",i)==i) equation.replace(i,sizeof("√")-1,"sqrt");
        else if(equation.find("∛",i)==i) equation.replace(i,sizeof("∛")-1,"cbrt");
        else if(equation.find("∜",i)==i) equation.replace(i,sizeof("∜")-1,"qtrt");
        else if(equation.find("⊕",i)==i) equation.replace(i,sizeof("⊕")-1,"XOR");
        else if(equation.find("∨",i)==i) equation.replace(i,sizeof("∨")-1,"OR");
        else if(equation.find("∧",i)==i) equation.replace(i,sizeof("∧")-1,"AND");
        else if(equation.find("≥",i)==i) equation.replace(i,sizeof("≥")-1,">=");
        else if(equation.find("−",i)==i) equation.replace(i,sizeof("−")-1,"-");    
        else if(equation.find("≠",i)==i) equation.replace(i,sizeof("≠")-1,"=!"); 
        else if(equation.find("÷",i)==i) equation.replace(i,sizeof("÷")-1,"/"); 
        else if(equation.find("×",i)==i) equation.replace(i,sizeof("×")-1,"*"); 
        else if(equation.find("π",i)==i) equation.replace(i,sizeof("π")-1,"pi");
        else if(equation.find("γ",i)==i) equation.replace(i,sizeof("γ")-1,"eul"); 
        else if(equation.find("ℯ",i)==i) equation.replace(i,sizeof("ℯ")-1,"e");
        else if(equation.find("ᵉ",i)==i) equation.replace(i,sizeof("ᵉ")-1,"ec");
        else if(equation.find("≈",i)==i) equation.replace(i,sizeof("≈")-1,"AROUND");
        else if(equation.find("H₀",i)==i) equation.replace(i,sizeof("H₀")-1,"H0");
        else if(equation.find("Z₀",i)==i) equation.replace(i,sizeof("Z₀")-1,"Z0");
        else if(equation.find("U₀",i)==i) equation.replace(i,sizeof("U₀")-1,"U0");
        else if(equation.find("mₐ",i)==i) equation.replace(i,sizeof("mₐ")-1,"ma");
    }
    for(size_t i{}; i<equation.length(); i++) if(equation.at(i)<32) equation.erase(i--,1); // Delete unprintable characters
    


    if(equation.find("fish")!=std::string::npos) // Fish.
    {                                   
        result+="fish.";         
        return 0;                       
    } 

    for(int i{}; i<equation.length(); i++)
    {
        if(!(isValidInput(equation.at(i)))) equation.erase(equation.begin()+i--); // Basic garbage removal
        if(i>=0)
        {
            if(globals::useDecimalComma && equation.at(i)==',') equation.at(i)='.';
            if(globals::useDecimalComma && equation.at(i)==';') equation.at(i)=',';
        }
    }
    
    if(equation.length()==0)
    {
        globals::error=true;
        result+="No valid input\n";
        equation.clear();
        return false;
    }
    std::vector<Token> tokens = getTokens(equation);
    if(!globals::errorMessage.empty() && !passedCalculationsFile)
    {
        globals::error=true;
        result=globals::errorMessage;
        globals::errorMessage.clear();
        resultAsOSStream.str("");
        resultAsOSStream.clear();
        equation.clear();
        tokens.clear();
        // globals::tokenMemory.clear();

        userVariables=globals::userVariables;    
        return false;        
    }

    bool hasX=containsX(equation);
    
    if(tokens.size()==0 && !options.graph) 
    {
        resultAsOSStream.str("");
        resultAsOSStream.clear();
        equation.clear();
        tokens.clear();
        globals::tokenMemory.clear();

        
        userVariables=globals::userVariables;
        return false;
    }


    if(!hasX && !options.graph) // No x found
    {
        resultAsOSStream<<calculation(tokens, cpp_complex<maxPrecision>(NAN,NAN));

        if(resultAsOSStream.str().find("nan")!=std::string::npos)
        {
            globals::ans="nan";
            resultAsOSStream.str("");
            resultAsOSStream.clear();
            resultAsOSStream<<"Not a Number";
        }
        else if(resultAsOSStream.str()=="-0")
        {
            globals::ans='0';
            resultAsOSStream.str("");
            resultAsOSStream.clear();
            resultAsOSStream<<"0";               
        }
        else globals::ans=resultAsOSStream.str();

        for(size_t i{}; i<tokens.size(); i++)
        {
            if (globals::options.prettyPrinting &&
                (tokens.at(i).value()=="<" || 
                    tokens.at(i).value()==">" ||
                    tokens.at(i).value()=="=" || 
                    tokens.at(i).value()=="=!" ||
                    tokens.at(i).value()=="<=" || 
                    tokens.at(i).value()=="OR" ||
                    tokens.at(i).value()=="AND" ||
                    tokens.at(i).value()=="AROUND" ||
                    tokens.at(i).value()=="XOR" ||
                    tokens.at(i).value()=="NOR" ||    
                    tokens.at(i).value()==">="))
            {
                if(resultAsOSStream.str()=="1") resultAsOSStream.str("true");
                else if(resultAsOSStream.str()=="0") resultAsOSStream.str("false");
            }
        }

        if(!passedCalculationsFile) result=resultAsOSStream.str();
        else
        {
            result+=equation+" = "+resultAsOSStream.str()+'\n';
        }
    }
    else if(!options.graph)
    {
        // result="";
        std::cout.precision(maxPrecision);
        resultAsOSStream.precision(maxPrecision);
        if(options.xStep==0)options.xStep=INFINITY;
        size_t i{};
        size_t totalCalculations = static_cast<size_t>(abs(options.xMax-options.xMin)/abs(options.xStep))+1;
        if(totalCalculations>100000)
        {
            options.xMax=options.xMin+options.xStep*100000;
            totalCalculations=100000;
        }

        for(cpp_complex<maxPrecision> xValue{options.xMin,0}; xValue.real()<=cpp_complex<maxPrecision>{options.xMax+0.000000001}.real(); xValue+=options.xStep)
        {
            i++;
            std::ostringstream xValueAsOSStream;
            if(xValue.real()>(-0.00002) && xValue.real()<0.00002 && options.xStep.real()>0.00002) xValue=0;
            xValueAsOSStream<<xValue;
            xValue=static_cast<cpp_complex<maxPrecision>>(xValueAsOSStream.str());

            // if(abs(xValue)-abs(round(xValue))<0.00001) xValue=round(xValue);

            resultAsOSStream<<calculation<cpp_complex<maxPrecision>>(tokens, xValue);
            if(resultAsOSStream.str().find("nan")!=std::string::npos)
            {
                resultAsOSStream.str("Not a Number");
            }
            if(resultAsOSStream.str()=="-0")
            {
                resultAsOSStream.str("");
                resultAsOSStream.clear();       
                resultAsOSStream<<"0";      
            }
            for(size_t i{}; i<tokens.size(); i++)
            {
                if (globals::options.prettyPrinting &&
                    (tokens.at(i).value()=="<" || 
                        tokens.at(i).value()==">" ||
                        tokens.at(i).value()=="=" || 
                        tokens.at(i).value()=="=!" ||
                        tokens.at(i).value()=="<=" ||
                        tokens.at(i).value()=="OR" || 
                        tokens.at(i).value()=="AND" || 
                        tokens.at(i).value()=="XOR" || 
                        tokens.at(i).value()=="AROUND" ||
                        tokens.at(i).value()=="NOR" || 
                        tokens.at(i).value()==">="))
                {
                    if(resultAsOSStream.str()=="1") resultAsOSStream.str("true");
                    else if(resultAsOSStream.str()=="0") resultAsOSStream.str("false");
                }
            }
            if(globals::decimalPrecision!=maxPrecision && isRealNumber(resultAsOSStream.str(),true))
            {
                // x-remainder(x,1/pow(10,decimalplaces))
                std::ostringstream oss;
                oss.precision(maxPrecision);
                oss<<evaluateRound<cpp_complex<maxPrecision>>(Token(std::string("round("+resultAsOSStream.str()+','+std::to_string(globals::decimalPrecision))), NAN, std::unordered_map<std::string, std::string>());
                resultAsOSStream.str("");
                resultAsOSStream<<oss.str();
            }
            
            if(globals::useDecimalComma && xValueAsOSStream.str().find('.')!=std::string::npos && !globals::error)
            {
                std::string xValueStr=xValueAsOSStream.str();
                for(size_t i{}; i<xValueStr.length(); i++) if(xValueStr.at(i)=='.') xValueStr.at(i)=',';
                xValueAsOSStream.str("");
                xValueAsOSStream<<xValueStr;
            }

            if(globals::useDecimalComma && resultAsOSStream.str().find('.')!=std::string::npos && !globals::error)
            {
                std::string resultStr=resultAsOSStream.str();
                for(size_t i{}; i<resultStr.length(); i++) if(resultStr.at(i)=='.') resultStr.at(i)=',';
                resultAsOSStream.str("");
                resultAsOSStream<<resultStr;
            }
            
            result+=std::to_string(i);
            for(size_t spaces{}; spaces<std::to_string(totalCalculations).length()-std::to_string(i).length(); spaces++) result+="  ";
            result+=": (" + xValueAsOSStream.str() + " ; " + resultAsOSStream.str()+")\n";
            resultAsOSStream.str("");
            resultAsOSStream.clear();
        }
    }
    else if(!passedCalculationsFile && options.graph)
    {
        globals::points.first.clear();
        globals::points.second.clear();

        if(hasX)
        {    
            uint threadCount{std::thread::hardware_concurrency()};
            std::vector<std::future<std::pair<std::vector<double>,std::vector<double>>>> results;
            results.reserve(threadCount);
            globals::points.first.reserve(static_cast<size_t>(abs(options.xMax-options.xMin).real()/options.xStep));
            // if(globals::options.xStep==0) globals::options.xStep=DBL_EPSILON;

            for(size_t i{}; i<threadCount; i++)
            {
                double perThreadRange{abs(options.xMax-options.xMin)/threadCount};
                double thisThreadOffset{perThreadRange*i};
                double thisThreadXValue{static_cast<double>(options.xMin+thisThreadOffset)};
                results.push_back(std::async(calculationCallerGraphing,std::ref(tokens),thisThreadXValue,perThreadRange+thisThreadXValue, i));
            }

            for(size_t i{}; i<threadCount; i++)
            {
                std::pair<std::vector<double>,std::vector<double>> thisThreadPoints=results.at(i).get();
                globals::points.first.append_range(thisThreadPoints.first);
                globals::points.second.append_range(thisThreadPoints.second);
            }    
        }
        else // Graphs without x are constant and thus only need to be calculated once.
        {
            std::complex<double> value = calculation<std::complex<double>>(tokens,NAN);
            globals::options.xStep*=6; // Less points
            size_t amount = static_cast<size_t>((globals::options.xMax.real()-globals::options.xMin.real())/globals::options.xStep.real())+3;
            double xValue = static_cast<double>(globals::options.xMin.real());
            globals::points.first.reserve(amount);
            globals::points.second.reserve(amount);

            for(size_t i{}; i<amount; i++) // A few extra points.
            {
                globals::points.first.emplace_back(xValue);
                if(value.imag()==0) globals::points.second.emplace_back(value.real());
                else globals::points.second.emplace_back(NAN);
                
                xValue+=static_cast<double>(globals::options.xStep.real());
            }
        }
        if(globals::debugCout)
        {
            std::cout<<"Points calculated for graph "<<equation<<" : ("<<globals::points.first.size()<<" ; "<<globals::points.second.size()<<")\n";
            globals::debugCoutUsed=true;
        }
    }    

    bool isKnownConstant{};
    if(!globals::errorMessage.empty() && !passedCalculationsFile)
    {
        globals::error=true;
        result=globals::errorMessage;
        globals::errorMessage.clear();
        goto cleanup;
    }

    // Show results equal to a constant as that constant
    if(!hasX && options.prettyPrinting && globals::valueToConstant.find(resultAsOSStream.str())!=globals::valueToConstant.end())
    {
        if(equation!=globals::valueToConstant.find(resultAsOSStream.str())->second)
        {
            if(!passedCalculationsFile) result=globals::valueToConstant.find(resultAsOSStream.str())->second;
            else
            {
                result+=equation+" = "+globals::valueToConstant.find(resultAsOSStream.str())->second+'\n';
            }
            isKnownConstant=true;
        }
    }

    // Check for fractions
    if(!hasX && 
        options.prettyPrinting && 
        isRealNumber(resultAsOSStream.str(),true) && 
        !isKnownConstant && 
        resultAsOSStream.str().find('e')==std::string::npos)
    {
        Frac frac = decimalToFraction(static_cast<cpp_complex<maxPrecision>>(resultAsOSStream.str()));
        
        std::string currentResult=resultAsOSStream.str();
        resultAsOSStream.str("");
        resultAsOSStream<<frac.numer<<'/'<<frac.denom;
        static_cast<cpp_complex<maxPrecision>>(currentResult).real();

        // Just try a couple times with more and more decimal places and see if the funny spits out a reasonable fraction lmao
        for(size_t i{10}; 
            (frac.numer==INFINITY || abs(calculation<cpp_complex<maxPrecision>>(getTokens(resultAsOSStream.str(),false,true), NAN) - static_cast<cpp_complex<maxPrecision>>(currentResult)).real() >= cpp_complex<maxPrecision>(0.000000000000000000000000000000001).real()) && 
            i<=20; i++)
        {
            frac = decimalToFraction(static_cast<cpp_complex<maxPrecision>>(currentResult),i);
            resultAsOSStream.str("");
            resultAsOSStream<<frac.numer<<'/'<<frac.denom;
        }

        if(equation!=resultAsOSStream.str() && 
            // std::fmod(frac.y,10)!=0 && // Stops something like 3.307 -> 3307/1000
            frac.denom!=1 &&
            abs(frac.numer)!=INFINITY &&
            abs(calculation<cpp_complex<maxPrecision>>(getTokens(resultAsOSStream.str(),false,true), NAN) - static_cast<cpp_complex<maxPrecision>>(currentResult).real()) < cpp_complex<maxPrecision>(0.000000000000000000000000000000001).real() ) 
        {
            if(!passedCalculationsFile) result=resultAsOSStream.str();
            else result+=equation+" = "+resultAsOSStream.str()+'\n';
        }
    }
    if(globals::decimalPrecision!=maxPrecision && !isKnownConstant && !hasX && !options.graph)
    {
        // Total hack. I don't care though.
        std::ostringstream oss;
        oss.precision(maxPrecision);
       
        oss<<evaluateRound<cpp_complex<maxPrecision>>(Token(std::string("round("+result+','+'('+std::to_string(globals::decimalPrecision)+','+std::to_string(globals::decimalPrecision)+"))")), NAN, std::unordered_map<std::string, std::string>());
        result=oss.str();
    }

    if(globals::useDecimalComma && result.find('.')!=std::string::npos && !globals::error && !passedInAsArg)
    {
        for(size_t i{}; i<result.length(); i++) if(result.at(i)==',') result.at(i)=';';
        for(size_t i{}; i<result.length(); i++) if(result.at(i)=='.') result.at(i)=',';
    }

    for(size_t i{1}; i<1000 && i<result.length()-2 && result.length()>2 && !globals::error && !passedInAsArg; i++)
    {
        if(std::isdigit(result.at(i-1)) && result.at(i)=='e' && (result.at(i+1)=='-' || result.at(i+1)=='+') && std::isdigit(result.at(i+2)))
        {
            result.replace(i,(result.at(i+1)=='-')+2*(result.at(i+1)=='+'),"×10^");
            i+=sizeof("×10^")+result.at(i+1)=='-';
        }
    }


    if(!hasX && !passedCalculationsFile && !options.graph)
    {
        // This caused me immense pain
        std::string addToHistory = '\n'+initialEquation+" = "+result;
        if(resultHistory.find(addToHistory)==std::string::npos) resultHistory+=addToHistory;
    }

    cleanup:
    globals::fnCallParams.clear();
    globals::prevUserFunctions=globals::userFunctions;
    resultAsOSStream.str("");
    resultAsOSStream.clear();
    equation.clear();
    tokens.clear();
    // globals::tokenMemory.clear();

    
    userVariables=globals::userVariables;
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline std::pair<std::vector<double>,std::vector<double>> calculationCallerGraphing(std::vector<Token> &tokens, double xValue, double xValueMax, size_t threadNumber)
{
    //if(threadNumber==std::thread::hardware_concurrency()-1) xValueMax+=static_cast<double>(globals::options.xStep);
    std::pair<std::vector<double>,std::vector<double>> points;
    points.second.reserve((xValueMax-xValue)/static_cast<double>(globals::options.xStep.real()));
    points.first.reserve((xValueMax-xValue)/static_cast<double>(globals::options.xStep.real()));
    for(;xValue<xValueMax; xValue+=static_cast<double>(globals::options.xStep.real()))
    {
        std::complex<double> result = calculation<std::complex<double>>(tokens,xValue);
        points.first.emplace_back(xValue);
        if(abs(result.imag())==0) points.second.emplace_back(result.real());
        else points.second.emplace_back(NAN);
    }
    return points;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline bool isValidInput(const char c)
{
    return !(c=='\t' || c=='\n' || c==' ' || c=='\\') && c>' ';
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline void parseMultiArgFunction(const std::string &input, std::vector<Token> &tokens, std::string functionName, size_t &i, bool &inFunctionCall, size_t argCount)
{
    size_t initialI{i};
    i=0;
    size_t argFound{1};
    std::string currentToken;
    int nestingLevel{};
    size_t functionNameLength{};
    functionNameLength=functionName.length();

    for(; i<input.length(); i++)
    {
        if(currentToken.empty() && (input.find(functionName, i)==i || functionName=="")) 
            for(; i<input.length(); i++)
            {
                if(!inFunctionCall)
                {
                    currentToken.append(functionName);
                    i+=functionNameLength;
                    inFunctionCall=true;
                    if(i==input.length()-1) continue;
                }
                if(i<input.length()-1 && input.at(i)==',' && nestingLevel==1 && input.at(i+1)!=')') argFound++;

                if((i<input.length()-1 && nestingLevel==0 && input.at(i)=='(' && input.at(i+1)==')') || 
                   (i==input.length()-1 && nestingLevel==0 && input.at(i)=='(')) argFound=0; // No arguments
                   
                if(input.at(i)==')') nestingLevel--;
                else if(input.at(i)=='(') nestingLevel++;
                currentToken.push_back(input.at(i));
                if(nestingLevel==0 || i==input.length()-1)
                {
                    if(argCount>argFound && !globals::passedCalculationsFile)
                    {
                        globals::errorMessage+="Too few arguments for function \"";
                        globals::errorMessage+=functionName;
                        globals::errorMessage+="\" (Expected at least "+ std::to_string(argCount) + ", found " + std::to_string(argFound) + ")\n";
                        globals::error=true;
                    }
                    else
                    {
                        tokens.emplace_back(currentToken);
                        i+=initialI;
                    }

                    return;
                }
            }
    }
    std::cout<<"How??\n";
    i+=initialI;
    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Pretty much a lexer.
inline std::vector<Token> getTokens(const std::string &input, bool resetFirstRun, bool noMemoize, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    
    for(std::pair<std::string,Function> fn : globals::userFunctions)
    {
        if(globals::prevUserFunctions.find(fn.first)==globals::prevUserFunctions.end())
        {
            noMemoize=true;
            globals::tokenMemory.clear();   
            break;
        }
        else if (fn.second.definition!=globals::userFunctions.at(fn.first).definition) 
        {
            noMemoize=true;
            globals::tokenMemory.clear();   
            break;
        }
    }
    if(!globals::options.graph) noMemoize=true;
    if(globals::tokenMemory.find(input)!=globals::tokenMemory.end() && !noMemoize)
    {
        return globals::tokenMemory.find(input)->second;
    }

    static bool firstRun{true};
    if(resetFirstRun)
    {
        firstRun=true;
        return std::vector<Token>();
    }
    
    int nestingLevel{};
    int absNestingLevel{};
    std::vector<Token> tokens{};
    
    std::string currentToken{};
    bool fixOffByOne{};
    bool inFunctionCall{};

    for(size_t i{}; i<input.length(); i++)
    {
        // Parse |x|... or ||x|| if the user hates me... or ||||x||||. whatever.
        if(input.at(i)=='|') for(;i<input.length(); i++)
        {
            if(!inFunctionCall)
            {
                for(;input.at(i)=='|' && i<input.length()-1;i++)
                {
                    absNestingLevel++;
                    currentToken.push_back('|');
                }
                inFunctionCall=true;
            }
            if(input.at(i)==')') nestingLevel--;
            else if(input.at(i)=='(') nestingLevel++;
            if(nestingLevel==0 && input.at(i)=='|') absNestingLevel--;
            if(absNestingLevel<0)
            {
                globals::errorMessage+="Bad absolute value parentheses\n";
                globals::error=true;
                break;
            }
            if(nestingLevel==0 && absNestingLevel==0 && nestingLevel==0 && input.at(i)=='|' || 
               (i==input.length()-1)) 
            {
                currentToken.push_back(input.at(i));
                tokens.emplace_back(currentToken);
                break;
            }
            else if(i==input.length()-1 && input.at(i)!='|') continue;

            currentToken.push_back(input.at(i));
        }

        // Parse MultiArg Functions
        for(size_t j{MAX_KEYWORD_LENGTH}; !inFunctionCall; j--)
        {
            if(j>input.length()-i) j=input.length()-i;
            if(j<2) break;
            token_t type{token_t::INVALID};
            size_t offset{};
            if(globals::multiArgFunctions.find(input.substr(i,j))!=globals::multiArgFunctions.end())
            {
                type=globals::multiArgFunctions.find(input.substr(i,j))->second;
                offset=globals::multiArgFunctions.find(input.substr(i,j))->first.length();

                if(i+offset<input.length() && input.at(i+offset)=='(')
                {
                    size_t minArgCount{static_cast<size_t>(-1)};
                    switch(type)
                    {
                        // case token_t::SMAX: minArgCount=2; break;
                        // case token_t::SMIN: minArgCount=2; break;
                        case token_t::RNDINT: minArgCount=2; break;
                        default:{}
                    }
                    parseMultiArgFunction(input.substr(i),tokens,input.substr(i,offset),i,inFunctionCall);
                }
                if(i>=input.length())
                {
                    globals::error=true;
                    return std::vector<Token>();
                }
                break;
            }
        }

        // Custom functions
        for(size_t j{MAX_KEYWORD_LENGTH}; !inFunctionCall && j>0; j--)
        {
            if(j>input.length()-i) j=input.length()-i;
            size_t offset{};
            if(globals::userFunctions.find(input.substr(i,j))!=globals::userFunctions.end() && input.substr(i,j)!=globals::options.definesFunction)
            {
                offset=globals::userFunctions.find(input.substr(i,j))->first.length();

                if(i+offset<input.length() && input.at(i+offset)=='(')
                {
                    currentToken+=globals::userFunctions.find(input.substr(i,j))->first;
                    for(size_t j{i+offset}; j<input.length(); j++)
                    {
                        if(input.at(j)==')') nestingLevel--;
                        else if(input.at(j)=='(') nestingLevel++;
                        if(nestingLevel!=0)currentToken.push_back(input.at(j));
                        if(nestingLevel==0 || j==input.length()-1)
                        {
                            i+=j-i;
                            break;
                        }
                    }
                }

                break;
            }
        }

        // Parse Subexpression (or complex number written as (real,imag) )
        if(currentToken.empty() && input.at(i)=='(') for(; i<input.length(); i++)
        {
            if(input.at(i)==')') nestingLevel--;
            else if(input.at(i)=='(') nestingLevel++;
            if(nestingLevel!=0)currentToken.push_back(input.at(i));
            if(nestingLevel==0 || i==input.length()-1) break;                
        }

        if(fnCallParams!=std::unordered_map<std::string, std::string>())
        {
            std::string candidate;
            int j{MAX_KEYWORD_LENGTH};
            if(j>input.length()-i) j=input.length()-i;
            for(; j>0; j--) // Check short substrs ahead of where you are in equation in descending size and match against function argument names
            {
                candidate = input.substr(i,j);
                
                if(fnCallParams.find(candidate)!=fnCallParams.end())
                {
                    currentToken=" ";
                    inFunctionCall=true;
                    tokens.emplace_back(std::pair<std::string,std::string>(candidate,fnCallParams.at(candidate)));
                    i+=j-1;
                    break;
                }
            }
        }
        
        // Parse other symbols
        if(currentToken.empty())
        {
            std::string candidate;
            size_t j{MAX_KEYWORD_LENGTH};
            if(j>input.length()-i) j=input.length()-i;
            for(; j>0; j--) // Check short substrs ahead of where you are in equation in descending size and match against known symbols
            {
                candidate = input.substr(i,j);
                if(globals::symbols.find(candidate)!=globals::symbols.end() || globals::userVariables.find(candidate)!=globals::userVariables.end())
                {
                    currentToken=candidate;
                    i+=j-1;
                    break;
                }
            }
        }

        // Parse Number
        if(currentToken.empty())for(; i<input.length() &&
                                  (std::isdigit(input.at(i)) || // 5
                                  
                                  (i<input.length()-1 && input.at(i)=='.' && std::isdigit(input.at(i+1))) || // .5

                                  (i>0 && std::isdigit(input.at(i-1)) &&
                                   input.at(i)=='e' && 

                                   ((i<input.length()-2 &&
                                   (((input.at(i+1)=='+' || input.at(i+1)=='-') &&
                                   std::isdigit(input.at(i+2))))) || 
                                   i<input.length()-1 && std::isdigit(input.at(i+1))))); i++) // 5e±5

                                   // I am deeply sorry.
        {
            fixOffByOne=true;
            if(i+1<input.length() &&
              (input.at(i)=='e' &&
              (input.at(i+1)=='+' || input.at(i+1)=='-')))
            {
                currentToken.push_back(input.at(i));
                i++;
            }
            currentToken.push_back(input.at(i));
        }
        if(fixOffByOne)
        {
            fixOffByOne=false;
            i--;
        }

        cleanup:
        if(inFunctionCall) currentToken.clear();
        if(!currentToken.empty()) tokens.emplace_back(currentToken);
        currentToken.clear();
        inFunctionCall=false;
    }
    if(!currentToken.empty()) tokens.emplace_back(currentToken);
    if(firstRun) firstRun=false;

    
    if(tokens.size()==0) return tokens;
    // Remove some stray operators
    for(size_t i{}; i<tokens.size(); i++)
    {
        if(tokens.at(0).category()==tokenCategory_t::OPERATOR && tokens.at(0).value()!="-") tokens.erase(tokens.begin());
        else break;
    }

    // Implicit parentheses around single argument functions
    {
        std::string newSubexpr="(";
        bool disallowMinus{};
        for(int i{1}; i<tokens.size(); i++)
        {
            if(tokens.at(i-1).type()==token_t::FUNCTION &&
                (
                    (tokens.at(i).category()==tokenCategory_t::NUMBER || tokens.at(i).type()==token_t::UNARYOP && tokens.at(i).value()!="-") || 
                    (tokens.at(i).value()=="-" && !disallowMinus && i<tokens.size()-1 && tokens.at(i+1).category()==tokenCategory_t::NUMBER) ||
                    ((tokens.at(i).value()=="**" || tokens.at(i).value()=="^") && i<tokens.size()-1 && tokens.at(i+1).category()==tokenCategory_t::NUMBER)
                )
                && (i==tokens.size()-1 || tokens.at(i).category()!=tokenCategory_t::SUBEXPR)
              )
            {
                disallowMinus=true;
                newSubexpr.append(tokens.at(i).value());
                tokens.erase(tokens.begin()+i);
                i--;
                
            }
            else if(newSubexpr.length()>1)
            {
                tokens.insert(tokens.begin()+i, Token(newSubexpr));
                newSubexpr="(";
                disallowMinus=false;
            }
            
        }
        if(newSubexpr.length()>1)
        {
            tokens.insert(tokens.end(), Token(newSubexpr));
        }
    }

    // Implicit multiplication, removing unary plus, so on

    for(size_t i{1}; i<tokens.size(); i++)
    {
        // Case example: 4!!3 -> 4!! h* 3
        if(tokens.at(i).category()==tokenCategory_t::NUMBER && 
        (tokens.at(i-1).type()==token_t::UNARYOP && tokens.at(i-1).value()!="-"))
                tokens.emplace(tokens.begin()+i++, Token("h*"));

        // Case example: 3x -> 3 h* x
        if((tokens.at(i).type()==token_t::VARIABLE || tokens.at(i).type()==token_t::CONSTANT || tokens.at(i).type()==token_t::SUMVAR) &&
            tokens.at(i-1).category()==tokenCategory_t::NUMBER) tokens.emplace(tokens.begin()+i++, Token("h*"));

        // Case example: x3 -> x h* 3
        if(tokens.at(i).category()==tokenCategory_t::NUMBER &&
        (tokens.at(i-1).type()==token_t::VARIABLE || tokens.at(i-1).type()==token_t::CONSTANT || tokens.at(i-1).type()==token_t::SUMVAR)) 
            tokens.emplace(tokens.begin()+i++, Token("h*"));

        // Case example: 3sin x -> 3 h* sinx
        if(tokens.at(i).type()==token_t::FUNCTION &&
        tokens.at(i-1).category()==tokenCategory_t::NUMBER) tokens.emplace(tokens.begin()+i++, Token("h*"));
        
        // Case example: 3(expr) -> 3 h* (expr)
        if((tokens.at(i).category()==tokenCategory_t::SUBEXPR || tokens.at(i).type()==token_t::FUNCTION) &&
            tokens.at(i-1).category()!=tokenCategory_t::OPERATOR &&
            tokens.at(i-1).type()!=token_t::FUNCTION) tokens.emplace(tokens.begin()+i++, Token("h*"));

        // Case example: (expr)3 -> (expr) h* 3 
        if(tokens.at(i-1).category()==tokenCategory_t::SUBEXPR && 
        tokens.at(i).category()!=tokenCategory_t::OPERATOR &&
        tokens.at(i).category()!=tokenCategory_t::SUBEXPR) tokens.emplace(tokens.begin()+i++, Token("h*"));

        // Case example: 3-3 -> 3+-3, (expr)-3 -> (expr)+-3
        // Reason: Binary minus is a lie lol
        if
        (tokens.at(i).value()=="-" &&
            ((tokens.at(i-1).category()==tokenCategory_t::NUMBER || tokens.at(i-1).category()==tokenCategory_t::SUBEXPR) ||
            (tokens.at(i-1).type()==token_t::UNARYOP && tokens.at(i-1).value()!="-"))
        ) tokens.emplace(tokens.begin()+i++, Token("+"));

        // Delete unary plus since it does jack
        if(tokens.at(i).value()=="+" &&
        tokens.at(i-1).category()!=tokenCategory_t::NUMBER &&
        (tokens.at(i-1).type()!=token_t::UNARYOP || tokens.at(i-1).value()=="-") &&
        tokens.at(i-1).category()!=tokenCategory_t::SUBEXPR) tokens.erase(tokens.begin()+i--);
    }

    for(int i{1}; i<tokens.size(); i++)
    {
        if(i<1) continue;
        if(tokens.at(i).type()==token_t::BINARYOP && tokens.at(i-1).type()==token_t::BINARYOP) // Example: 3**/3 -> 3**3
        {
            tokens.erase(tokens.begin()+i--);
        }
        if(i>0 && tokens.at(i).value()=="-" && tokens.at(i-1).value()=="-")
        {
            tokens.erase(tokens.begin()+i-1,tokens.begin()+i+1);
            i-=2;
        }
    }
    for(int i{static_cast<int>(tokens.size())-1}; i>=0 ; i--)
    {
        if(tokens.at(i).value()=="-" ||
        tokens.at(i).type()==token_t::BINARYOP ||
        tokens.at(i).type()==token_t::FUNCTION) tokens.erase(tokens.begin()+i);

        else break;
    }


    if(globals::errorMessage.empty() && !noMemoize && fnCallParams==std::unordered_map<std::string, std::string>()) globals::tokenMemory.emplace(input,tokens);
    return tokens;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
T calculation(std::vector<Token> tokens, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    if constexpr((std::is_same_v<T,cpp_complex<maxPrecision>> || std::is_same_v<T,std::complex<double>>))
    {
        if(tokens.size()==0) return NAN;
        std::ostringstream resultAsOSStream;
        resultAsOSStream.precision(std::numeric_limits<T>::digits10);
            
        // Replace ans, rnd, rndint with numbers
        for(size_t i{}; i<tokens.size(); i++)
        {
            if(tokens.at(i).type()==token_t::CONSTANT)
            {
                if(tokens.at(i).value()=="ans")
                {
                    tokens.at(i)=Token(globals::ans);
                }
                else if(tokens.at(i).value()=="rnd" || tokens.at(i).value()=="rndint")
                {
                    std::uniform_real_distribution<> doubleDist(0,1);
                    resultAsOSStream<<doubleDist(randomMt);
                    std::string randomAsStr {resultAsOSStream.str()};
                    if(tokens.at(i).value()=="rndint") // To get random integers, it literally deletes the decimal point
                    {
                        randomAsStr.erase(randomAsStr.find('.'), 1);
                    }
                    tokens.at(i)=Token(randomAsStr);
                    resultAsOSStream.str("");
                    resultAsOSStream.clear();
                }
            }
        }

        resultAsOSStream.str("");
        resultAsOSStream.clear();

        if(tokens.size()==1 && tokens.at(0).category()==tokenCategory_t::NUMBER) return tokens.at(0).number(xValue);
        if(tokens.size()==1 && tokens.at(0).type()==token_t::INVALID) return NAN;

        size_t pass{};
        for(; pass<=LOGICALS && !globals::error; pass++)
        {
            for(int i{}; i<tokens.size() && !globals::error; i++) // Stop trying when error found
            {
                if(pass==SUBEXPRESSIONS)
                {
                    if(tokens.at(i).category()==tokenCategory_t::SUBEXPR)
                    {
                        T evaluatedSubexpr{NAN};
                        switch(tokens.at(i).type())
                        {
                            case token_t::SUBEXPR: evaluatedSubexpr = calculation<T>(getTokens(tokens.at(i).value(),false,false,fnCallParams), xValue, fnCallParams); break;
                            case token_t::ABS:     evaluatedSubexpr = evaluateAbs(tokens.at(i), xValue, fnCallParams); break;
                            case token_t::DIFF:    evaluatedSubexpr = evaluateDiff(tokens.at(i), xValue, fnCallParams); break;
                            case token_t::MEAN:    evaluatedSubexpr = evaluateMean(tokens.at(i), xValue, fnCallParams); break;
                            case token_t::MEDIAN:  evaluatedSubexpr = evaluateMedian(tokens.at(i), xValue, fnCallParams); break;
                            case token_t::STDEVP:  evaluatedSubexpr = evaluateStdevp(tokens.at(i), xValue, fnCallParams); break;
                            case token_t::MAX:     evaluatedSubexpr = evaluateMax(tokens.at(i), xValue, fnCallParams); break;
                            case token_t::MIX:     evaluatedSubexpr = evaluateMix(tokens.at(i), xValue, fnCallParams); break;
                            case token_t::CUSTOMFN: evaluatedSubexpr = evaluateCustomFn(tokens.at(i),xValue, fnCallParams); break;
                            case token_t::SABS:    evaluatedSubexpr = evaluateSabs(tokens.at(i), xValue, fnCallParams); break;
                            // case token_t::SMAX:    evaluatedSubexpr = evaluateSmax(tokens.at(i), xValue); break;
                            // case token_t::SMIN:    evaluatedSubexpr = evaluateSmin(tokens.at(i), xValue); break;
                            case token_t::ATAN2:     evaluatedSubexpr = evaluateAtan2(tokens.at(i), xValue, fnCallParams); break;
                            case token_t::GCF:     evaluatedSubexpr = evaluateGcf(tokens.at(i), xValue, fnCallParams); break;
                            case token_t::LCM:     evaluatedSubexpr = evaluateLcm(tokens.at(i), xValue, fnCallParams); break;
                            case token_t::MIN:     evaluatedSubexpr = evaluateMin(tokens.at(i), xValue, fnCallParams); break;
                            case token_t::RNDSEL:  evaluatedSubexpr = evaluateRndsel(tokens.at(i), xValue, fnCallParams); break;
                            case token_t::RNDINT:  evaluatedSubexpr = evaluateRndint(tokens.at(i), xValue, fnCallParams); break;
                            case token_t::ROUND:  evaluatedSubexpr = evaluateRound(tokens.at(i), xValue, fnCallParams); break;
                            case token_t::TRUNC:  evaluatedSubexpr = evaluateTrunc(tokens.at(i), xValue, fnCallParams); break; 
                            case token_t::ROOT:    evaluatedSubexpr = evaluateRoot(tokens.at(i), xValue, fnCallParams); break;
                            case token_t::LOG:     evaluatedSubexpr = evaluateLog(tokens.at(i), xValue, fnCallParams); break;
                            case token_t::IF:      evaluatedSubexpr = evaluateIf(tokens.at(i), xValue, fnCallParams); break;
                            default:{}                
                        }
                        tokens.at(i)=Token(evaluatedSubexpr);
                    }
                }
                else if (pass==FUNCTIONS && i!=0)
                {

                    if((tokens.at(i-1).type()==token_t::FUNCTION) && tokens.at(i).category()==tokenCategory_t::NUMBER)
                    {
                        T evaluatedUnary=evaluateUnary<T>(tokens.at(i), tokens.at(i-1), xValue);

                        if(!globals::options.graph && (tokens.at(i-1).value()=="sin" || tokens.at(i-1).value()=="cos") && abs(evaluatedUnary)<0.00000000000001)
                        {
                            evaluatedUnary=0;
                        }

                        tokens.at(i-1)=Token(evaluatedUnary);
                        tokens.erase(tokens.begin()+i);
                        i--;
                    }
                }
                else if(pass==UNARYOPS && i!=0)
                {
                    if((tokens.at(i).type()==token_t::UNARYOP || tokens.at(i).type()==token_t::UNARYOP) && tokens.at(i-1).category()==tokenCategory_t::NUMBER)
                    {
                        T evaluatedUnary=evaluateUnary<T>(tokens.at(i-1), tokens.at(i), xValue);
                        tokens.at(i-1)=Token(evaluatedUnary);
                        tokens.erase(tokens.begin()+i);
                        i--;
                    }
                }
                else if(pass==EXPONENTIATION && i==0)
                {
                    for(i=tokens.size()-1; i>0; i--)
                    {
                        if(i-2<tokens.size())
                        {
                            // Account for something like x^-1
                            if((tokens.at(i-2).value()=="^" || tokens.at(i-2).value()=="**") && tokens.at(i-1).value()=="-" && tokens.at(i).category()==tokenCategory_t::NUMBER)
                            {
                                T evaluatedUnary=evaluateUnary<T>(tokens.at(i), tokens.at(i-1), xValue);
                                tokens.at(i-1)=Token(evaluatedUnary);
                                tokens.erase(tokens.begin()+i);                        
                            }
                            if(tokens.at(i-2).category()==tokenCategory_t::NUMBER && (tokens.at(i-1).value()=="^" || tokens.at(i-1).value()=="**") && tokens.at(i).category()==tokenCategory_t::NUMBER)
                            {
                                T evaluatedBinary=evaluateBinary<T>(tokens.at(i-2), tokens.at(i-1), tokens.at(i), xValue);
                                tokens.at(i-2)=Token(evaluatedBinary);
                                tokens.erase(tokens.begin()+i-1,tokens.begin()+i+1);
                            }
                        }
                    }
                }
                else if (pass==UNARYMINUS)
                {
                    if(i!=0 && (tokens.at(i-1).value()=="-") && tokens.at(i).category()==tokenCategory_t::NUMBER)
                    {
                        T evaluatedUnary=evaluateUnary<T>(tokens.at(i), tokens.at(i-1), xValue);
                        tokens.at(i-1)=Token(evaluatedUnary);
                        tokens.erase(tokens.begin()+i);
                        i--;
                    }
                }

                else if(pass==MULTIPLICATIONIMPLICIT && globals::options.prioritizeImplicitMultiplication && i>1)
                {
                    if(tokens.at(i-2).category()==tokenCategory_t::NUMBER && (tokens.at(i-1).value()=="h*") && tokens.at(i).category()==tokenCategory_t::NUMBER)
                    {
                        T evaluatedBinary=evaluateBinary(tokens.at(i-2), tokens.at(i-1), tokens.at(i), xValue);
                        tokens.at(i-2)=Token(evaluatedBinary);
                        tokens.erase(tokens.begin()+i-1,tokens.begin()+i+1);
                        i-=2;
                    }                
                }

                else if(pass==MULTIPLICATION && i>1)
                {
                    if(tokens.at(i-2).category()==tokenCategory_t::NUMBER && 
                    (globals::opToPriority.find(tokens.at(i-1).value()))!=globals::opToPriority.end() &&
                    (globals::opToPriority.find(tokens.at(i-1).value())->second == pass || tokens.at(i-1).value()=="h*") && 
                    tokens.at(i).category()==tokenCategory_t::NUMBER)
                    {
                        T evaluatedBinary=evaluateBinary(tokens.at(i-2), tokens.at(i-1), tokens.at(i), xValue);
                        tokens.at(i-2)=Token(evaluatedBinary);
                        tokens.erase(tokens.begin()+i-1,tokens.begin()+i+1);
                        i-=2;
                    }
                }
                else if(pass==ADDITION && i>1)
                {
                    if(tokens.at(i-2).category()==tokenCategory_t::NUMBER && (tokens.at(i-1).value()=="+") && tokens.at(i).category()==tokenCategory_t::NUMBER)
                    {
                        T evaluatedBinary=evaluateBinary(tokens.at(i-2), tokens.at(i-1), tokens.at(i), xValue);
                        tokens.at(i-2)=Token(evaluatedBinary);
                        tokens.erase(tokens.begin()+i-1,tokens.begin()+i+1);
                        i-=2;
                    }
                } 
                else if(pass==COMPARISONS && i>1)
                {
                    if(tokens.at(i-2).category()==tokenCategory_t::NUMBER && 
                    (globals::opToPriority.find(tokens.at(i-1).value()))!=globals::opToPriority.end() &&
                    globals::opToPriority.find(tokens.at(i-1).value())->second == pass &&
                    tokens.at(i).category()==tokenCategory_t::NUMBER)
                    {
                        T evaluatedBinary=evaluateBinary(tokens.at(i-2), tokens.at(i-1), tokens.at(i), xValue);
                        tokens.at(i-2)=Token(evaluatedBinary);
                        tokens.erase(tokens.begin()+i-1,tokens.begin()+i+1);
                        i-=2;
                    }
                }
                else if(pass==LOGICALS && i>1)
                {
                    if(tokens.at(i-2).category()==tokenCategory_t::NUMBER && 
                    (globals::opToPriority.find(tokens.at(i-1).value()))!=globals::opToPriority.end() &&
                    globals::opToPriority.find(tokens.at(i-1).value())->second == pass &&
                    tokens.at(i).category()==tokenCategory_t::NUMBER)
                    {
                        T evaluatedBinary=evaluateBinary(tokens.at(i-2), tokens.at(i-1), tokens.at(i), xValue);
                        tokens.at(i-2)=Token(evaluatedBinary);
                        tokens.erase(tokens.begin()+i-1,tokens.begin()+i+1);
                        i-=2;
                    }
                }
            }
        }
        if(tokens.size()==1 && tokens.at(0).number(xValue)==T(-0)) tokens.at(0)=Token("0");
        if(tokens.size()==1 && tokens.at(0).type()==token_t::VARIABLE) return xValue;
        if(tokens.size()==1 && (tokens.at(0).type()==token_t::NUMBER|| tokens.at(0).type()==token_t::CONSTANT)) return tokens.at(0).number(xValue);
        

        // if(globals::errorMessage=="" && !globals::passedCalculationsFile) globals::errorMessage+="Malformed expression\n";
        // globals::error=true;
    }
    return NAN;
}

template<typename T>
T evaluateAbs( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    return abs(calculation<T>(getTokens(arg.value()), xValue));
}

template <typename T>
bool evaluateArgs( const Token &arg, const T xValue, std::vector<T>&argVals, const std::unordered_map<std::string, std::string> &fnCallParams, size_t argsToEval)
{
    if(globals::error) return true;
    std::string currentToken;
    int nestingLevel{};
    for(size_t i{}; i<arg.value().length() && nestingLevel>=0 && argVals.size()<argsToEval; i++)
    {
        if(globals::error) return true;
        if(arg.value().at(i)=='(') nestingLevel++;
        else if(arg.value().at(i)==')') nestingLevel--;
        if(nestingLevel<0) break;
        if(!(arg.value().at(i)==',' && nestingLevel==0) && i<arg.value().length()) currentToken.push_back(arg.value().at(i));
        else
        {
            argVals.emplace_back(calculation<T>(getTokens(currentToken,false,false, fnCallParams), xValue, fnCallParams));
            currentToken.clear();
        }
    }
    if(!currentToken.empty()) 
    {
        argVals.emplace_back(calculation<T>(getTokens(currentToken,false,false, fnCallParams), xValue, fnCallParams));
    }
    if(globals::error) return true;
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T evaluateMean( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    T result{};
    std::vector<T> argVals;
    if(evaluateArgs(arg, xValue, argVals, fnCallParams)) return NAN; 
    for(size_t i{}; i<argVals.size(); i++)
    {
        result+=argVals.at(i);
    }
    result=result/T(argVals.size());

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T evaluateMedian( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    if constexpr((std::is_same_v<T,cpp_complex<maxPrecision>> || std::is_same_v<T,std::complex<double>>))
    {    
        std::vector<T> argVals;
        if(evaluateArgs(arg, xValue, argVals, fnCallParams)) return NAN;

        std::sort(argVals.begin(), argVals.end(), [](T a, T b){return a.real()<b.real();});

        if(argVals.size()%2!=0) return argVals.at(argVals.size()/2);
        else return (argVals.at(argVals.size()/2-1)+argVals.at(argVals.size()/2))/T(2);
    }
    else return T();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T evaluateStdevp( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    if constexpr((std::is_same_v<T,cpp_complex<maxPrecision>> || std::is_same_v<T,std::complex<double>>))
    {    
        std::vector<T> argVals;
        if(evaluateArgs(arg, xValue, argVals, fnCallParams)) return NAN;

        std::sort(argVals.begin()+1, argVals.end(), [](T a, T b){return a.real()<b.real();});

        if(argVals.size()<2)
        {
            return T(0);
        }
        T summedIntermediates{};
        for(size_t i{}; i<argVals.size(); i++)
        {
            summedIntermediates+=argVals.at(i);
        }
        const T mean{summedIntermediates/T(argVals.size())};
        T summed{};
        for(size_t i{}; i<argVals.size(); i++)
        {
            summed+=pow(argVals.at(i)-mean,2);
        }


        return sqrt(summed/T(argVals.size())); // Intellegre
    }
    else return T();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T evaluateLcm( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    if constexpr((std::is_same_v<T,cpp_complex<maxPrecision>> || std::is_same_v<T,std::complex<double>>))
    {    
        std::ostringstream numberAsOSStream;
        if constexpr(std::is_same_v<T,cpp_complex<maxPrecision>>)
        {
            numberAsOSStream.precision(maxPrecision);
        }
        else numberAsOSStream.precision(std::numeric_limits<double>::digits10);
        std::vector<T> argVals;
        T tempValue{};
        T numLeft{};
        T numRight{};
        std::string numberAsString;
        if(evaluateArgs(arg, xValue, argVals, fnCallParams)) return NAN;
        for(size_t i{}; i<argVals.size(); i++)
        {
            argVals.at(i).imag(0);
        }
        if(globals::options.graph)
        {
            for(size_t i{}; i<argVals.size(); i++) argVals.at(i)=round(argVals.at(i).real());
        }
        for(size_t i{}; i<argVals.size(); i++)
        {
            if(argVals.at(i).real()<0) argVals.at(i).real(-argVals.at(i).real());
        }
        if(argVals.size()==1) return argVals.at(0);
        numLeft=argVals.at(0); //a
        numRight=argVals.at(1); //b
        while(argVals.at(1).real()!=0)
        {
            tempValue=argVals.at(1); //b
            argVals.at(1)=fmod(argVals.at(0).real(),argVals.at(1).real());
            argVals.at(0)=tempValue; 
        }
        argVals.at(0)=(numLeft*numRight)/argVals.at(0);
        argVals.erase(argVals.begin()+1);
        if(argVals.size()>=2)
        {
            numberAsOSStream<<"lcm(";
            for(size_t i{}; i<argVals.size(); i++)
            {
                numberAsOSStream<<argVals.at(i).real();
                numberAsOSStream<<',';
            }
            Token newArg{numberAsOSStream.str()};
            argVals.at(0) = evaluateLcm(newArg,xValue, fnCallParams);
        }
        return argVals.at(0);
    }
}

// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T evaluateGcf( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    if constexpr((std::is_same_v<T,cpp_complex<maxPrecision>> || std::is_same_v<T,std::complex<double>>))
    {    
        std::ostringstream numberAsOSStream;
        std::vector<T> argVals;
        T tempValue{};
        if(evaluateArgs(arg, xValue, argVals, fnCallParams)) return NAN;
        if(globals::options.graph)
        {
            for(size_t i{}; i<argVals.size(); i++) argVals.at(i)=round(argVals.at(i).real());
        }
        for(size_t i{}; i<argVals.size(); i++)
        {
            if(argVals.at(i).real()<0) argVals.at(i)=-argVals.at(i);
        }
        while(argVals.size()>=2)
        {
            while(argVals.at(1).real()>0)
            {
                tempValue=argVals.at(1);
                argVals.at(1).real(fmod(argVals.at(0).real(),argVals.at(1).real()));
                argVals.at(1).imag(fmod(argVals.at(0).imag(),argVals.at(1).imag()));
                argVals.at(0)=tempValue; 
            }
            argVals.erase(argVals.begin()+1);
        }
        return argVals.at(0);
    }
}

// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T = cpp_dec_float_100>
T evaluateRndint( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    if constexpr((std::is_same_v<T,cpp_complex<maxPrecision>> || std::is_same_v<T,std::complex<double>>))
    {    
        std::vector<T> argVals;
        std::string currentToken;
        int nestingLevel{};
        if(evaluateArgs(arg, xValue, argVals, fnCallParams,2)) return NAN;
        
        if(argVals.size()==1) return argVals.at(0).real();

        if(argVals.at(0)!=argVals.at(0) || argVals.at(1)!=argVals.at(1)) return NAN; // Check for NAN

        if(argVals.at(0).real() > argVals.at(1).real()) std::swap(argVals.at(0), argVals.at(1));

        std::uniform_int_distribution<> intDist(static_cast<int>(argVals.at(0).real()),static_cast<int>(argVals.at(1).real()));
        return T(intDist(randomMt),0);
    }
}

// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T evaluateRndsel( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    if constexpr((std::is_same_v<T,cpp_complex<maxPrecision>> || std::is_same_v<T,std::complex<double>>))
    {    
        std::vector<T> argVals;
        if(evaluateArgs(arg, xValue, argVals, fnCallParams)) return NAN;
        std::uniform_int_distribution<size_t> intDist(0, argVals.size()-1);
        return argVals.at(intDist(randomMt));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T evaluateMax( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    std::vector<T> argVals;
    if(evaluateArgs(arg, xValue, argVals, fnCallParams)) return NAN;

    for(size_t i{}; i<argVals.size(); i++) if(argVals.at(i).imag()>argVals.at(0).imag()) argVals.at(0).imag(argVals.at(i).imag());
    for(size_t i{}; i<argVals.size(); i++) if(argVals.at(i).real()>argVals.at(0).real()) argVals.at(0).real(argVals.at(i).real());
    return argVals.at(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T evaluateDiff( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    if constexpr((std::is_same_v<T,cpp_complex<maxPrecision>> || std::is_same_v<T,std::complex<double>>))
    {    
        std::vector<T> argVals;
        std::string currentToken;
        int nestingLevel{};
        T diffStepSize = static_cast<T>(globals::options.xStep);
        if(diffStepSize.real()<0.05) diffStepSize=0.05;
        for(size_t i{}; i<arg.value().length() && nestingLevel>=0 && argVals.size()<2; i++)
        {
            if(globals::error) return NAN;
            if(arg.value().at(i)=='(') nestingLevel++;
            else if(arg.value().at(i)==')') nestingLevel--;
            if(nestingLevel<0) break;
            if(!(arg.value().at(i)==',' && nestingLevel==0) && i<arg.value().length()) currentToken.push_back(arg.value().at(i));
            else
            {
                argVals.emplace_back(calculation<T>(getTokens(currentToken), xValue+diffStepSize, fnCallParams));
                argVals.emplace_back(calculation<T>(getTokens(currentToken), xValue-diffStepSize, fnCallParams));
                currentToken.clear();
                break;
            }
        }
        if(!currentToken.empty())
        {
            argVals.emplace_back(calculation<T>(getTokens(currentToken), xValue+diffStepSize, fnCallParams));
            argVals.emplace_back(calculation<T>(getTokens(currentToken), xValue-diffStepSize, fnCallParams));
            currentToken.clear();  
        }
        return (argVals.at(0)-argVals.at(1))/(diffStepSize.real()*T(2));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T evaluateMin( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    std::vector<T> argVals;
    if(evaluateArgs(arg, xValue, argVals, fnCallParams)) return NAN;

    for(size_t i{}; i<argVals.size(); i++) if(argVals.at(i).imag()<argVals.at(0).imag()) argVals.at(0).imag(argVals.at(i).imag());
    for(size_t i{}; i<argVals.size(); i++) if(argVals.at(i).real()<argVals.at(0).real()) argVals.at(0).real(argVals.at(i).real());
    return argVals.at(0);
}

template <typename T>
T evaluateMix( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    std::vector<T> argVals;
    std::string currentToken;
    if(evaluateArgs(arg, xValue, argVals, fnCallParams,3)) return NAN;
    
    else if(argVals.size()<3) return argVals.at(0);
    else if(argVals.at(2).real()>=1) return argVals.at(1);
    else if(argVals.at(2).real()<=0) return argVals.at(0);
    
    else return argVals.at(0)*(T(1,0)-argVals.at(2))+argVals.at(1)*argVals.at(2);
}

template <typename T> T evaluateAtan2( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    std::vector<T> argVals;
    std::string currentToken;
    if(evaluateArgs(arg, xValue, argVals, fnCallParams,2)) return NAN;

    if(argVals.size()<2) return NAN;

    return atan2(argVals.at(1).real(),argVals.at(0).real());

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T evaluateSabs( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    std::vector<T> argVals;
    std::string currentToken;
    if(evaluateArgs(arg, xValue, argVals, fnCallParams,2)) return NAN;
    
    if(argVals.size()<2) argVals.emplace_back(0.1); // Default argument
    T enumerator = argVals.at(1)-abs(argVals.at(0));
    if(enumerator.real()<0) enumerator.real(0);
    return abs(argVals.at(0))+(pow(enumerator,2)/(T(2)*argVals.at(1)));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T evaluateIf( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    if constexpr((std::is_same_v<T,cpp_complex<maxPrecision>> || std::is_same_v<T,std::complex<double>>))
    {    
        std::vector<T> argVals;
        std::string currentToken;
        if(evaluateArgs(arg, xValue, argVals, fnCallParams,3)) return NAN;
        
        if(argVals.size()==1) return argVals.at(0).real();
        if(argVals.size()==2)
        {
            if(argVals.at(0).real()) return argVals.at(1);
            else return NAN;
        }
        else 
        {
            if(argVals.at(0).real()) return argVals.at(1);
            else return argVals.at(2);
        }
    }
    else return T();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T evaluateRound( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    std::vector<T> argVals;
    if(evaluateArgs(arg, xValue, argVals, fnCallParams,2)) return NAN;
    
    if(argVals.size()==1 || (argVals.at(1).real()<1 && argVals.at(1).imag()<1)) return T(round(argVals.at(0).real()),round(argVals.at(0).imag()));

    if(argVals.at(1).real()>maxPrecision) argVals.at(1).real(maxPrecision);
    else if(argVals.at(1).real()<0) argVals.at(1).real(0);

    if(argVals.at(1).imag()>maxPrecision) argVals.at(1).imag(maxPrecision);
    else if(argVals.at(1).imag()<0) argVals.at(1).imag(0);
    
    argVals.at(1)=T(floor(argVals.at(1).real()),floor(argVals.at(1).imag()));
    argVals.at(0)=argVals.at(0)-T(remainder(argVals.at(0).real(),1/pow(10,argVals.at(1).real())),remainder(argVals.at(0).imag(),1/pow(10,argVals.at(1).imag())));
    if(argVals.at(0).real()==-0) argVals.at(0).real(0);
    if(argVals.at(0).imag()==-0) argVals.at(0).imag(0);
    return argVals.at(0);

}
// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T evaluateTrunc( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{

    std::vector<T> argVals;
    if(evaluateArgs(arg, xValue, argVals, fnCallParams,2)) return NAN;
    
    if(argVals.size()==1 || (argVals.at(1).real()<1 && argVals.at(1).imag()<1)) return T(trunc(argVals.at(0).real()),trunc(argVals.at(0).imag()));

    if(argVals.at(1).real()>maxPrecision) argVals.at(1).real(maxPrecision);
    else if(argVals.at(1).real()<0) argVals.at(1).real(0);

    if(argVals.at(1).imag()>maxPrecision) argVals.at(1).imag(maxPrecision);
    else if(argVals.at(1).imag()<0) argVals.at(1).imag(0);
    
    argVals.at(1)=T(floor(argVals.at(1).real()),floor(argVals.at(1).imag()));
    return argVals.at(0)-T(fmod(argVals.at(0).real(),1/pow(10,argVals.at(1).real())),fmod(argVals.at(0).imag(),1/pow(10,argVals.at(1).imag())));
    
    // return argVals.at(0)-fmod(argVals.at(0),1/pow(10,argVals.at(1)));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T evaluateRoot( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    if constexpr((std::is_same_v<T,cpp_complex<maxPrecision>> || std::is_same_v<T,std::complex<double>>))
    {    
        std::vector<T> argVals;
        std::string currentToken;
        if(evaluateArgs(arg, xValue, argVals, fnCallParams,2)) return NAN;
        
        if(argVals.size()==1) return sqrt(argVals.at(0)); // Default: sqrt()
        
        return pow(argVals.at(0), T(1)/argVals.at(1));
    }
    else return T();
}

// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T evaluateLog( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    if constexpr((std::is_same_v<T,cpp_complex<maxPrecision>> || std::is_same_v<T,std::complex<double>>))
    {    
        std::vector<T> argVals;
        if(evaluateArgs(arg, xValue, argVals, fnCallParams,2)) return NAN;
        
        if(argVals.size()==1) argVals.emplace(argVals.begin(),10); // Default argument
        return log(argVals.at(1))/log(argVals.at(0));
    }
    else return T();
}

template <typename T> T evaluateCustomFn( const Token &arg, const T xValue, const std::unordered_map<std::string, std::string> &fnCallParams)
{
    if constexpr((std::is_same_v<T,cpp_complex<maxPrecision>> || std::is_same_v<T,std::complex<double>>))
    {    
        size_t offset{};
        for(size_t i{MAX_KEYWORD_LENGTH}; i>0; i--)
        {
            if(globals::userFunctions.find(arg.value().substr(0,i))!=globals::userFunctions.end())
            {
                offset=i;
                break;
            }
        }

        Function fn = globals::userFunctions.find(arg.value().substr(0,offset))->second;
        if(offset==0) return NAN;
        if(offset>=arg.value().length()-1) return NAN;
        std::vector<T> argVals;
        std::string currentToken;
        int nestingLevel{-1};
        Token argWithoutName{arg.value().substr(offset+1)};
        if(evaluateArgs(argWithoutName, xValue, argVals, fnCallParams,fn.argc)) return NAN;
        
        std::unordered_map<std::string,std::string> paramVariables{};

        std::ostringstream oss;
        for(size_t i{}; i<argVals.size(); i++)
        {
            std::string paramName=fn.argNames.at(i);
            for(size_t i{}; i<paramName.length(); i++)
            {
                char c = paramName.at(i);
                if(paramName.length()==1 && c=='x') break;
                if((c=='\t' || c=='\n' || c=='\\' || c=='(' || c==')' || c=='x') || c<=' ' || i>=MAX_KEYWORD_LENGTH)
                {
                    return NAN;
                }
            }
        
            oss.str("");
            if constexpr(std::is_same_v<T,std::complex<double>>)
            {
                oss.precision(15);
            }
            else oss.precision(maxPrecision);

            oss<<argVals.at(i);
            if(paramVariables.find(paramName)!=paramVariables.end()) return NAN; // Bad definition, multiple params with same name
            else paramVariables.emplace(paramName, oss.str());
            
        }

        if(argVals.size()>=fn.argc || true)
        {
            std::vector<Token> tokens = getTokens(globals::userFunctions.find(arg.value().substr(0,offset))->second.definition,false,true,paramVariables);
            return calculation<T>(tokens, NAN,paramVariables);
        }
    }
    return NAN;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
T evaluateBinary(Token &numberTokenLeft, Token &operation, Token &numberTokenRight, const T xValue)
{
    if(globals::error) return NAN;
    T x{numberTokenLeft.number(xValue)};
    T y{numberTokenRight.number(xValue)};
    if(globals::opToID.find(operation.value()) == globals::opToID.end()) return NAN;
    size_t id = globals::opToID.find(operation.value())->second;

    if constexpr((std::is_same_v<T,cpp_complex<maxPrecision>> || std::is_same_v<T,std::complex<double>>))
        switch(id) // This goes against every principle.
        {
            case 0: return x+y;
            case 1: return x*y;
            case 2: return x/y;
            case 3: return x*y;
            case 4:
            {
                if(y!=floor(y.real()) || abs(y.real())>INT_MAX) return pow(x, y);
                else return pow(x,static_cast<int>(y.real()));
            }
            case 5: return x-y*floor(x.real()/y.real());
            case 6: return x.real()<y.real();
            case 7: return x.real()>y.real();
            case 8: return x==y;
            case 10: return fmod(x.real(),y.real());
            case 11: return remainder(x.real(),y.real());
            case 12: 
            {
                if(x.real()>=y.real()) return (tgamma(x.real()+1)/tgamma(x.real()-y.real()+1));
                else return NAN;
            }
            case 13:
            {
                if(x.real()>=y.real()) return (tgamma(x.real()+1)/tgamma(x.real()-y.real()+1));
                else return NAN;
            }

            case 15: return x.real()&&y.real();
            case 16: return (!x.real())!=(!y.real());
            case 17: return abs(x-y)<=globals::options.aroundTruthinessLeniency.real();
            case 18: return (x.real()==0)&&(y.real()==0);
            case 19: return x.real()||y.real();
            case 20: return x!=y;
            case 21: return x.real()>=y.real();
            case 22: return x.real()<=y.real();
            case 105: return floor(x.real()/y.real());
            default: return NAN;
        }

    return NAN;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T>
T evaluateUnary(Token &numberToken, Token &operation, const T xValue)
{
    if(globals::error) return NAN;
    if(globals::opToID.find(operation.value()) == globals::opToID.end()) return NAN;
    size_t id = globals::opToID.find(operation.value())->second;
    T x=numberToken.number(xValue);

    if constexpr((std::is_same_v<T,cpp_complex<maxPrecision>> || std::is_same_v<T,std::complex<double>>)) // Jank
        switch(id) // :horror:
        {
            case 23: return tgamma(x.real()+1);
            case 24: return -x;
            case 25: 
            {
                // This sucks but apparently the radix of cpp_complex<MAX_PRECISION>, based on cpp_bin_float_100, is 0, so the constant thingy complains if I don't do it this way.
                if constexpr(std::is_same_v<T,cpp_complex<maxPrecision>>)
                {
                    return pow(T(2)/cpp_complex<maxPrecision>(boost::math::constants::pi<cpp_bin_float_100>(),0.),T(1)/4.*(T(1)-cos(cpp_complex<maxPrecision>(boost::math::constants::pi<cpp_bin_float_100>(),0.)*x)))*pow(2,x/T(2))*tgamma(x.real()/2+1);
                }
                else return pow(T(2)/boost::math::constants::pi<double>(),T(1)/4.*(T(1)-cos(boost::math::constants::pi<double>()*x)))*pow(2,x/T(2))*tgamma(x.real()/2+1);
            }
            case 26: 
            {
                if(x.real()!=0) return sin(x)/x;
                else return 1;
            }
            case 27: 
            {
                if(x.real()!=0) return pow(sin(x)/x,2);
                else return 1;
            }
            case 28: 
            {
                Token e{"e"};
                return pow(e.number(xValue),x);            
            }
            case 29:
            {
                if(x.real()>0) return 1;
                if(x.real()<0) return -1;
                else return 0;
            }
            case 30: return sqrt(x);
            case 31: return pow(x,1./3);
            case 32: return pow(x,0.25);


            case 33: return sin(x);
            case 34: return cos(x);
            case 35: return tan(x);

            case 36: return sinh(x);
            case 37: return cosh(x);
            case 38: return tanh(x);


            case 39: return asin(x);
            case 40: return acos(x);
            case 41: return atan(x);

            case 42: return asinh(x);
            case 43: return acosh(x);
            case 44: return atanh(x);


            case 45: return T(1)/cos(x);
            case 46: return T(1)/sin(x);
            case 47: return T(1)/tan(x);

            case 48: return T(1)/cosh(x);
            case 49: return T(1)/sinh(x);
            case 50: return T(1)/tanh(x);

            case 51: return acos(T(1)/x);
            case 52: return asin(T(1)/x);
            case 53: return atan(T(1)/x);

            case 54: return acosh(T(1)/x);
            case 55: return asinh(T(1)/x); // aschhschhshuhuschush
            case 56: return atanh(T(1)/x);

            case 57: // prime
            {
                x=floor(x.real());
                if(x.real()<=1) return false;
                else if(x.real() == 2 || x.real() == 3) return true;
                else if(fmod(x.real(),2)==0 || fmod(x.real(),3)==0) return false;
                for(size_t i{5}; i*i<=x.real(); i+=6)
                {
                    if(fmod(x.real(),i) == 0 || fmod(x.real(),i+2) == 0) return false;
                }
                return true;            
            }
            case 58: return log(x);
            case 59: return pow(log(x),2);
            case 60: return abs(x);
            case 61: return floor(x.real());
            case 62: return trunc(x.real());
            case 63: return ceil(x.real());
            case 64:
            {
                if(x+0.5 == round(x.real()) && fmod(floor(x.real()),2)==0)
                {
                    return floor(x.real());
                }
                else return round(x.real());            
            }
            case 65: return round(x.real());
            case 66: return (1+abs(x)-abs(x-T(1)))/2;
            case 67: return (x+abs(x))/T(2);
            case 68: 
            {
                if(x.real()>1) return 1;
                else if(x.real()<0) return 0;
                return pow(x,2)*(T(3)-T(2)*x);            
            }
            case 69: return lgamma(x.real());
            case 70: return (x);


            case 71: return pow(sin(x),2);
            case 72: return pow(cos(x), 2);
            case 73: return pow(tan(x),2);

            case 74: return pow(sinh(x),2);
            case 75: return pow(cosh(x),2);
            case 76: return pow(tanh(x),2);

            case 77: return pow(asin(x),2);
            case 78: return pow(acos(x),2);
            case 79: return pow(atan(x),2);

            case 80: return pow(asinh(x),2);
            case 81: return pow(acosh(x),2);
            case 82: return pow(atanh(x),2);

            case 83: return pow(T(1)/cos(x),2);
            case 84: return pow(T(1)/sin(x),2);
            case 85: return pow(T(1)/tan(x),2);

            case 86: return pow(T(1)/cosh(x),2);
            case 87: return pow(T(1)/sinh(x),2);
            case 88: return pow(T(1)/tanh(x),2);

            case 89: return pow(acos(T(1)/x),2);
            case 90: return pow(asin(T(1)/x),2);
            case 91: return pow(atan(T(1)/x),2);

            case 92: return pow(acosh(x),2);
            case 93: return pow(asinh(x),2);
            case 94: return pow(atanh(T(1)/x),2);

            case 95: return x.real();
            case 96: return x.imag();
            case 97: return arg(x);
            case 98: return norm(x);
            case 99: return conj(x);
            case 100: return proj(x);
            case 101: return cos(x)+T(0,1)*sin(x);
            case 102: return pow(cos(x)+T(0,1)*sin(x),2);
            case 103: return sin(x)+cos(x);
            case 104: return pow(sin(x)+cos(x),2);

            
            default: std::cout<<"Declared, but undefined unary operator used. Somehow.\n";
        }
    return T();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// The GOAT
inline bool isRealNumber(const std::string &input, bool disallowSpecials)
{
    if(input.empty()) return false;
    if(!disallowSpecials)
    {
        if(input=="inf") return true;
        if(input=="-inf") return true;
        if(input=="nan") return true;
        if(input=="-nan") return true;
    }
    if(input=="-") return false;
    if(input=="e") return false;

    for(size_t i{}; i<input.length(); i++) if((input.at(i)<'0' || input.at(i)>'9') && 
                                               input.at(i)!='e' && 
                                               input.at(i)!='.' &&
                                               input.at(i)!='+' &&
                                               input.at(i)!='-') return false;
    size_t dotCount{};
    size_t eCount{};
    size_t exponentLength{};
    bool seenMinus{};
    

    for(size_t i{}; i<input.length() && eCount<2; i++)
    {
        if(eCount) exponentLength++;
        if(exponentLength>9) return false; // Avoids a really pissy crash.
        if((seenMinus && input.at(i)=='-')||(i>0 && input.at(i)=='-' && input.at(i-1)!='e')) return false;
        if(input.at(0)=='-' && i==0 && input.length()>1)
        {
            seenMinus=true;
            continue;
        }
        if(input.at(i)=='e') 
        {
            if(i+2<input.length() && input.at(i)=='e' && (input.at(i+1)=='+' || input.at(i+1)=='-') && std::isdigit(input.at(i+2))) i+=2;
            eCount++;
        }
        if(input.at(i)=='.')
        {
            dotCount++;
            if(i==input.length()-1) return false;
            if(eCount) 
            {
                if(!globals::passedCalculationsFile) globals::errorMessage+="Number \"" + input + "\" can only have an integer exponent\n";
                globals::error=true;
                return false;
            }
            if(dotCount>1)
            {
                if(!globals::passedCalculationsFile) globals::errorMessage+="Number \"" + input + "\" has multiple decimal points\n";
                globals::error=true;
                return false;
            }
        }

        if(!isNumberPart(input.at(i))) return false;
    }
    if(eCount>1) return false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline bool isNumberPart(const char input)
{
    return (input>='0' && input<='9') || input=='.' || input=='e';
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


inline long long unguardedGcd(long long a, long long b)
{
    if(b==0) return a;
    else return unguardedGcd(b,a%b);
}

inline Frac decimalToFraction(cpp_complex<maxPrecision> enumerator, size_t precision)
{
    const bool negative{enumerator.real()<0};
    if(negative) enumerator=-enumerator;
    std::string fraction;
    std::string number;
    std::ostringstream asOSStream;
    asOSStream.precision(15);
    if constexpr (std::is_same<cpp_complex<maxPrecision>,cpp_complex<maxPrecision>>()) asOSStream.precision(precision);
    asOSStream<<enumerator;
    number=asOSStream.str();
    cpp_complex<maxPrecision> denominator{1};

    for(size_t i{}; enumerator!=round(enumerator.real()) && enumerator==enumerator; i++)
    {
        enumerator*=10;
        denominator*=10;
    }

    size_t length = number.substr(number.find('.')+1).length()-1;
    bool hasTriedTwice{};
    retry:
    std::string pattern=number.substr(number.find('.')+1,length/2.f);
    size_t occurences{};
    for(size_t i{number.find('.')+1}; i<number.length() && !pattern.empty() && length>=11; i++)
    {
        if(number.find(pattern,i)==i)
        {
            occurences++;
            i+=pattern.length()-1;
        }
    }
    if(occurences<=1 && length>=12 && !hasTriedTwice)
    {
        length-=1;
        hasTriedTwice=true;
        goto retry;
    }

    
    if(occurences>1)
    {
        if constexpr (std::is_same<double, cpp_complex<maxPrecision>>() || std::is_same<long double, cpp_complex<maxPrecision>>()) 
        {
            enumerator=std::stold(number.substr(0,number.find('.')));
        }
        else enumerator=static_cast<cpp_complex<maxPrecision>>(number.substr(0,number.find('.')));
        denominator=pow(10,pattern.length())-1;
        enumerator=(enumerator*denominator)+std::stoll(pattern);
    }
    long long enumeratori=static_cast<long long>(enumerator);
    long long denominatori=static_cast<long long>(denominator);
    long long gcd = unguardedGcd(enumeratori,denominatori);


    enumerator/=gcd;
    denominator/=gcd;

    if(negative) enumerator=-enumerator;

    return Frac(static_cast<double>(enumerator),static_cast<double>(denominator));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool isNameValid(std::string &name, bool modifyName=false);

inline bool addVariable(const std::pair<std::string,std::string> &newVariable, std::unordered_map<std::string, std::string>& variableContainer)
{
    globals::options= Options{false,
                                    0,
                                    0,
                                    0,
                                    0,
                                    false,
                                    0,
                                    "",
                                    0,
                                    ""};
    std::string name = newVariable.first;
    if(!lessetB::isNameValid(name)) return true;
    globals::oss.precision(maxPrecision);
    globals::oss.str("");
    if(globals::symbols.find(name)!=globals::symbols.end()) return true;
    cpp_complex<maxPrecision> value = calculation<cpp_complex<maxPrecision>>(getTokens(newVariable.second,false,true),NAN);
    globals::oss<<value;
    if(variableContainer.find(name)!=variableContainer.end()) variableContainer.find(name)->second=globals::oss.str();
    else variableContainer.emplace(name, globals::oss.str());
    globals::tokenMemory.clear();
    return false;
}


inline bool isNameValid(std::string &name, bool modifyName)
{
    bool allDigits{true};
    for(size_t i{}; i<name.length(); i++)
    {
        char c = name.at(i);
        if(!std::isdigit(c)) allDigits=false;
        if((c=='\t' || c=='\n' || c=='\\' || c=='(' || c==')' || c=='x') || c<=' ' || i>=MAX_KEYWORD_LENGTH)
        {
            if(!modifyName) return false;
            name.erase(i--,1);
        }
    }
    if(globals::symbols.find(name)!=globals::symbols.end() || name=="h*")
    {
        if(globals::constants.find(name)==globals::constants.end()) return false;
    }
    if(allDigits) return false;
    return true;
}

inline bool containsX(const std::string &equation)
{
    if(equation.length()>=1 && equation.at(0)=='x') return true;
    for(int i{}; i<equation.length(); i++)
    {
        if(i==1 && equation.at(1)=='x' && equation.find("exp",0)!=0) return true;
        if(i>1&&equation.at(i)=='x' && equation.find("mix",i-2)!=i-2 && equation.find("max",i-2)!=i-2 && equation.find("exp",i-1)!=i-1) return true;
    }
    return false;
}

inline Function getFnFromArgs(const std::string &def, std::string &sig)
{

    std::string argName;
    std::vector<std::string> argNames;
    if(sig.find('(')==std::string::npos || sig.find(')')==std::string::npos) return Function();

    {
        size_t lparenCount{};
        size_t rparenCount{};
        for(size_t i{}; i<sig.length(); i++)
        {
            if(sig.at(i)=='(') lparenCount++;
            else if(sig.at(i)==')') rparenCount++;
        }
        if(lparenCount!=1 || rparenCount!=1) return Function();
    }

    for(size_t i{sig.find('(')+1}; i<sig.length() && sig.at(i)!=')'; i++)
    {
        char c = sig.at(i);
        if(!isValidInput(c))
        {
            sig.erase(i,1);
            continue;
        }
        if(c!=',') argName.push_back(c);
        else 
        {
            argNames.emplace_back(argName);
            argName.clear();
        }
    }
    if(!argName.empty()) argNames.emplace_back(argName);
    for(std::string name : argNames)
    {
        if(globals::symbols.find(name)!=globals::symbols.end() || globals::multiArgFunctions.find(name)!=globals::multiArgFunctions.end())
        {
            return Function();
        }
    }
    Function fn{def,argNames,argNames.size()};
    return fn;
}

inline Function getFnFromSignature(const std::string &sig)
{
    {
        size_t lparenCount{};
        size_t rparenCount{};
        for(size_t i{}; i<sig.length() && sig.at(i)!='='; i++)
        {
            if(sig.at(i)=='(') lparenCount++;
            else if(sig.at(i)==')') rparenCount++;
        }
        if(lparenCount!=1 || rparenCount!=1) return Function();
    }

    if(sig.find('=') == std::string::npos) return Function();

    std::string argName;
    std::vector<std::string> argNames;
    for(size_t i{sig.find('(')+1}; i<sig.length(); i++)
    {
        char c = sig.at(i);
        if(c==')') break;
        if(!isValidInput(c)) return Function();
        if(c!=',') argName.push_back(c);
        else
        {
            argNames.emplace_back(argName);
            argName.clear();
        }
    }
    if(!argName.empty()) argNames.emplace_back(argName);
    for(std::string name : argNames)
    {
        if(globals::symbols.find(name)!=globals::symbols.end() || globals::multiArgFunctions.find(name)!=globals::multiArgFunctions.end())
        {
            return Function();
        }
    }
    Function fn{sig.substr(sig.find('=')+1),argNames,argNames.size()};
    return fn;
}

/*
3+(pi/root(2+4,10-2))-25x

3: Number                                               -> NUMBER
+: BinaryOp                                             -> OPERATOR
(pi/root(2+4,10-2)): SubExpr                            -> SUBEXPR
    pi: Constant (Will later be replaced by Number)     -> CONSTANT
    /: BinaryOp                                         -> OPERATOR
    root(2+4,10-2): Root                                -> SUBEXPR
        argVals:
            [0]
                2: Number                               -> NUMBER   
                +: BinaryOp                             -> OPERATOR
                4: Number                               -> NUMBER
            [1]
                10: Number                              -> NUMBER    
                -: UnaryOp (Will later be treated as +-)-> OPERATOR
                2: Number                               -> NUMBER
-:UnaryOp (Will later be treated as +-)                 -> OPERATOR
25:Number                                               -> NUMBER
x:Variable (Will later be replaced by Number)           -> NUMBER

*/
/*
    Grammar: (Subexpr could also be variable or constant)
    NUMBER||SUBEXPR then SUBEXPR||UNARYOP
    NUMBER||SUBEXPR then BINARYOP then NUMBER||SUBEXPR
    ANY then SUBEXPR
    SUBEXPR then ANY
*/   
}