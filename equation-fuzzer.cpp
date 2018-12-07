/* By Guido Vranken <guidovranken@gmail.com> */

#include <cstdio>
#include <string>
#include <random>
#include <sstream>
#include <boost/optional.hpp>
#include <iostream>

#include "exprtk.hpp"


template<const int NumNumbers>
class EquationFuzzer {
    static_assert(NumNumbers > 0 && NumNumbers <= 26, "0 < NumNumbers <= 26");
    private:
        using NumStruct =
            struct {
                double numbers[NumNumbers];
        };

        const bool round;
        const std::vector<std::string> conditions;
        const std::string expr1;
        const std::string expr2;
        size_t iter = 0;

        std::default_random_engine prng;
        std::uniform_real_distribution<double> unifDouble;
        std::uniform_int_distribution<size_t> unifSizeT;
        std::vector<NumStruct> corpus;

        double getRandomDouble() {
            double ret = 0;
            while ( !ret ) {
                ret = unifDouble(prng);
                if ( !ret ) {
                    continue;
                }
            }
            return ret;
        }

        double calc(const std::string expressionStr, const NumStruct& ns) const {
            typedef exprtk::symbol_table<double> symbol_table_t;
            typedef exprtk::expression<double>     expression_t;
            typedef exprtk::parser<double>             parser_t;

            NumStruct nsCopy = ns;
            symbol_table_t symbol_table;
            {
                for (size_t i = 0; i < NumNumbers; i++) {
                    symbol_table.add_variable(toString(i), nsCopy.numbers[i]);
                }
            }
            symbol_table.add_constants();

            expression_t expression;
            expression.register_symbol_table(symbol_table);

            parser_t parser;

            if ( !parser.compile(expressionStr, expression) ) {
                std::cout << "Error: " << parser.error() << std::endl;
                std::cout << "Expression: " << expressionStr << std::endl;
                exit(1);
            }

            return expression.value();
        }

        std::string toString(const int numIndex) const {
            std::stringstream ss;

            const char curVarName = 'a' + numIndex;
            ss << curVarName;

            return ss.str();
        }

        std::string toString(const NumStruct& ns, const std::string separator = ",") const {
            std::stringstream ss;

            for (size_t i = 0; i < NumNumbers; i++) {
                ss << toString(i) << "=" << ns.numbers[i];
                if ( i != NumNumbers -1 ) {
                    ss << separator;
                }
            }

            return ss.str();
        }

        std::string toScript(const NumStruct& ns) const {
            std::stringstream ss;

            ss << toString(ns, "\n") << "\n";
            ss << expr1 << " == " << expr2 << "\n";

            return ss.str();
        }

        double numStructToDiff(const NumStruct& ns, double& res1, double& res2) {
            res1 = calc(expr1, ns);
            res2 = calc(expr2, ns);
            iter++;
            return std::abs(res1 > res2 ? res1 - res2 : res2 - res1);
        }

        NumStruct mutate(void) {
            while ( true ) {
                NumStruct ns = getStatic();

                for (size_t j = 0; j < 3; j++) {
                    /* Select a number to mutate */
                    const size_t whichNum = unifSizeT(prng) % NumNumbers;

                    /* Select a mutator to use */
                    const size_t whichMutator = unifSizeT(prng) % 2;

                    switch ( whichMutator ) {
                        case    0:
                            {
                                ns.numbers[whichNum] += getRandomDouble();
                            }
                            break;
                        case    1:
                            {
                                ns.numbers[whichNum] -= getRandomDouble();
                            }
                            break;
                    }

                    if ( round == true ) {
                        double z;
                        ns.numbers[whichNum] -= std::modf(ns.numbers[whichNum], &z);
                    }
                }

                return ns;
            }
        }

        NumStruct getStatic(void) {
            /* corpus always has at least 1 element, added in the constructor */
            return corpus[ unifSizeT(prng) % corpus.size() ];
        }

    public:
        EquationFuzzer(
                const std::string expr1,
                const std::string expr2,
                const std::vector<std::string> conditions,
                const bool round = true) :
            round(round),
            conditions(conditions),
            expr1(expr1),
            expr2(expr2),
            unifDouble(-100, 100) {
                corpus.push_back({});
        }

        void Fuzz(void) {
            boost::optional<double> diff;

            while ( true ) {
                const NumStruct ns = mutate();

                /* Test conditions */
                {
                    bool doContinue = false;
                    for (const auto& C : conditions) {

                        if ( calc(C, ns) != 1.0f ) {
                            /* Condition test failed */
                            doContinue = true;
                            break;
                        }
                    }
                    if ( doContinue == true ) {
                        /* Generate a new number set */
                        continue;
                    }
                }

                double res1, res2;
                const double curDiff = numStructToDiff(ns, res1, res2);

                if (    !diff /* diff not yet defined */ ||
                        (diff && curDiff < *diff) ) {
                    corpus.push_back(ns);
                    diff = curDiff;

                    std::cout << "N: " << iter << " ";
                    std::cout << "Corp: " << corpus.size() << " ";
                    std::cout << "Vars: " << toString(ns) << " ";
                    std::cout << "Res1: " << res1 << " ";
                    std::cout << "Res2: " << res2 << " ";
                    std::cout << "Diff: " << *diff << std::endl;

                    if ( *diff == 0.0f ) {
                        std::cout << "The solution to" << std::endl << std::endl;
                        std::cout << "    " << expr1 << " == " << expr2 << std::endl << std::endl;
                        if ( conditions.size() ) {
                            std::cout << "under these conditons:" << std::endl << std::endl;
                            for (const auto& C : conditions) {
                                std::cout << "    " << C << std::endl;
                            }
                            std::cout << std::endl;
                        }
                        std::cout << "is:" << std::endl << std::endl;
                        std::cout << "    " << toString(ns) << std::endl << std::endl;

                        std::cout << "Script:" << std::endl << std::endl;
                        std::cout << toScript(ns) << std::endl << std::endl;
                        return;
                    }
                }
            }
        }
};

int main(int argc, char** argv)
{
    const int NumNumbers = 6;

    if ( argc <= 2 ) {
        std::cout << "Usage: " << argv[0] << " EXPR1 EXPR2 [CONDITIONS]" << std::endl;
        std::cout << std::endl;
        std::cout << "The program will attempt to resolve variables such that EXPR1 == EXPR2" << std::endl;
        std::cout << std::endl;
        return 0;
    }

    std::vector<std::string> conditions;

    for (int i = 3; i < argc; i++) {
        conditions.push_back( argv[i] );
    }

    EquationFuzzer<NumNumbers> F(argv[1], argv[2], conditions);

    F.Fuzz();

    return 0;
}
