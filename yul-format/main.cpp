#include <libyul/AsmParser.h>
#include <libyul/AsmPrinter.h>
#include <libyul/AssemblyStack.h>
#include <libyul/Dialect.h>
#include <libyul/backends/evm/EVMDialect.h>

#include <liblangutil/ErrorReporter.h>
#include <liblangutil/SourceReferenceFormatterHuman.h>

#include <libdevcore/CommonIO.h>

#include <cstdlib>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>

using boost::optional;
using namespace std;

namespace po = boost::program_options;

boost::optional<std::string> prettyPrint(
	std::string const& _sourceCode,
	std::string const& _sourceName,
	yul::EVMDialect const& _evmDialect,
	langutil::ErrorReporter& _errorReporter
)
{
	langutil::EVMVersion const evmVersion = *langutil::EVMVersion::fromString("petersburg");
	yul::EVMDialect const& evmDialect = yul::EVMDialect::strictAssemblyForEVM(evmVersion);
	yul::Parser parser{_errorReporter, evmDialect};
	langutil::CharStream source{_sourceCode, _sourceName};
	auto scanner = make_shared<langutil::Scanner>(source);
	shared_ptr<yul::Block> ast = parser.parse(scanner, true);

	if (_errorReporter.hasErrors())
		return boost::none;
	else
		return {yul::AsmPrinter{true}(*ast)};
}

int main(int argc, const char* argv[])
{
	langutil::ErrorList errors;
	langutil::ErrorReporter errorReporter{errors};

	string sourceName{"input.yul"};
	string sourceCode{
		R"({ for { let a := 1 } iszero(eq(a, 10)) { a := add(a, 1) } { a := add(a, 1)
			a := add(a, 2) a := add(a, 3)
			a := add(a, 4) a := add(a, 5)
			a := add(a, 6) a := add(a, 7)
			} })"
	};

	po::options_description options(
		R"(yul-format, the Yul source code pretty printer.
Usage: yul-format [Options] < input
Reads a single source from stdin and prints it with proper formatting.

Allowed options)",
		po::options_description::m_default_line_length,
		po::options_description::m_default_line_length - 23);
	options.add_options()
		("help", "Show this help screen.")
		(
			"evm-version",
			po::value<string>()->value_name("version"),
			"Select desired EVM version. Either homestead, tangerineWhistle, spuriousDragon, byzantium, constantinople or petersburg (default)."
		)
		("input-file", po::value<vector<string>>(), "input file");
	po::positional_options_description filesPositions;
	filesPositions.add("input-file", -1);

	po::variables_map arguments;
	try
	{
		po::command_line_parser cmdLineParser(argc, argv);
		cmdLineParser.options(options).positional(filesPositions);
		po::store(cmdLineParser.run(), arguments);
	}
	catch (po::error const& _exception)
	{
		cerr << _exception.what() << endl;
		return 1;
	}

	if (arguments.count("help"))
		cout << options;
	else
	{
		string input;

		if (arguments.count("input-file"))
			for (string path: arguments["input-file"].as<vector<string>>())
				input += dev::readFileAsString(path);
		else
			input = dev::readStandardInput();

		langutil::EVMVersion evmVersion = *langutil::EVMVersion::fromString("petersburg");
		if (arguments.count("evm-version"))
		{
			string versionOptionStr = arguments["evm-version"].as<string>();
			boost::optional<langutil::EVMVersion> value = langutil::EVMVersion::fromString(versionOptionStr);
			if (!value)
			{
				cerr << "Invalid option for --evm-version: " << versionOptionStr << endl;
				return EXIT_FAILURE;
			}
			evmVersion = *value;
		}
		yul::EVMDialect const& evmDialect = yul::EVMDialect::strictAssemblyForEVM(evmVersion);
		optional<string> const pretty = prettyPrint(sourceCode, sourceName, evmDialect, errorReporter);

		if (!errorReporter.hasErrors())
			cout << *pretty;
		else
			for (shared_ptr<langutil::Error const> const& error : errors)
				langutil::SourceReferenceFormatterHuman{cerr, true}.printErrorInformation(*error);
	}

	return EXIT_SUCCESS;
}
