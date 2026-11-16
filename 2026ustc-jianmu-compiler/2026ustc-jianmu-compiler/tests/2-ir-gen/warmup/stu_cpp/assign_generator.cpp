#include "BasicBlock.hpp"
#include "Constant.hpp"
#include "Function.hpp"
#include "IRBuilder.hpp"
#include "Module.hpp"

#include <iostream>

int main() {
	auto module = new Module();
	auto builder = new IRBuilder(nullptr, module);
	auto int32 = module->get_int32_type();
	auto main_fun = Function::create(FunctionType::get(int32, {}), "main", module);
	auto entry = BasicBlock::create(module, "entry", main_fun);
	builder->set_insert_point(entry);

	auto array = builder->create_alloca(ArrayType::get(int32, 10));
	auto first = builder->create_gep(array, {ConstantInt::get(0, module), ConstantInt::get(0, module)});
	builder->create_store(ConstantInt::get(10, module), first);
	auto second = builder->create_gep(array, {ConstantInt::get(0, module), ConstantInt::get(1, module)});
	auto product = builder->create_imul(builder->create_load(first), ConstantInt::get(2, module));
	builder->create_store(product, second);
	builder->create_ret(builder->create_load(second));

	std::cout << module->print();
	delete module;
}