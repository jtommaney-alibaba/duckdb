#include "duckdb/function/table/read_csv.hpp"
#include "duckdb/execution/operator/persistent/buffered_csv_reader.hpp"
#include "duckdb/function/function_set.hpp"

using namespace std;

namespace duckdb {

struct ReadCSVFunctionData : public TableFunctionData {
	ReadCSVFunctionData() {
	}

	//! The CSV reader
	unique_ptr<BufferedCSVReader> csv_reader;
};

static unique_ptr<FunctionData> read_csv_bind(ClientContext &context, vector<Value> inputs,
                                              vector<SQLType> &return_types, vector<string> &names) {
	for (auto &val : inputs[2].struct_value) {
		names.push_back(val.first);
		if (val.second.type != TypeId::VARCHAR) {
			throw BinderException("read_csv requires a type specification as string");
		}
		return_types.push_back(TransformStringToSQLType(val.second.str_value.c_str()));
	}
	if (names.size() == 0) {
		throw BinderException("read_csv requires at least a single column as input!");
	}
	auto result = make_unique<ReadCSVFunctionData>();

	BufferedCSVReaderOptions options;
	options.auto_detect = false;
	options.file_path = inputs[0].str_value;
	options.header = false;
	options.delimiter = inputs[1].str_value;

	result->csv_reader = make_unique<BufferedCSVReader>(context, move(options), return_types);
	return move(result);
}

static unique_ptr<FunctionData> read_csv_auto_bind(ClientContext &context, vector<Value> inputs,
                                                   vector<SQLType> &return_types, vector<string> &names) {
	

	auto result = make_unique<ReadCSVFunctionData>();
	BufferedCSVReaderOptions options;
	options.auto_detect = true;
	options.file_path = inputs[0].str_value;

	if (inputs.size() == 3) {
		options.sample_size = inputs[1].CastAs(TypeId::INT64).value_.bigint;
		if (options.sample_size > STANDARD_VECTOR_SIZE) {
			throw BinderException(
				"Unsupported parameter for SAMPLE_SIZE: cannot be bigger than STANDARD_VECTOR_SIZE %d",
				STANDARD_VECTOR_SIZE);
		}
		options.num_samples = inputs[2].CastAs(TypeId::INT64).value_.bigint;
	}

	result->csv_reader = make_unique<BufferedCSVReader>(context, move(options));

	// TODO: print detected dialect from result->csv_reader->info
	return_types.assign(result->csv_reader->sql_types.begin(), result->csv_reader->sql_types.end());
	names.assign(result->csv_reader->col_names.begin(), result->csv_reader->col_names.end());

	return move(result);
}

static void read_csv_info(ClientContext &context, vector<Value> &input, DataChunk &output, FunctionData *dataptr) {
	auto &data = ((ReadCSVFunctionData &)*dataptr);
	data.csv_reader->ParseCSV(output);
}

void ReadCSVTableFunction::RegisterFunction(BuiltinFunctions &set) {
	TableFunctionSet read_csv("read_csv");
	read_csv.AddFunction(TableFunction({SQLType::VARCHAR, SQLType::VARCHAR, SQLType::STRUCT}, read_csv_bind, read_csv_info, nullptr));
	read_csv.AddFunction(TableFunction({SQLType::VARCHAR}, read_csv_auto_bind, read_csv_info, nullptr));
	read_csv.AddFunction(TableFunction({SQLType::VARCHAR, SQLType::VARCHAR, SQLType::VARCHAR}, read_csv_auto_bind,
	                                   read_csv_info, nullptr));
	set.AddFunction(read_csv);

	TableFunctionSet read_csv_auto("read_csv_auto");
	read_csv_auto.AddFunction(TableFunction({SQLType::VARCHAR}, read_csv_auto_bind, read_csv_info, nullptr));
	read_csv_auto.AddFunction(TableFunction({SQLType::VARCHAR, SQLType::VARCHAR, SQLType::VARCHAR},
	                                        read_csv_auto_bind,
	                                        read_csv_info, nullptr));
	set.AddFunction(read_csv_auto);
}

void BuiltinFunctions::RegisterReadFunctions() {
	CSVCopyFunction::RegisterFunction(*this);
	ReadCSVTableFunction::RegisterFunction(*this);
}

} // namespace duckdb
