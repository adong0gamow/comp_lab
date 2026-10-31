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

	auto callee_type = FunctionType::get(int32, {int32});
	auto callee = Function::create(callee_type, "callee", module);
	auto callee_entry = BasicBlock::create(module, "entry", callee);
	builder->set_insert_point(callee_entry);
	auto &arg = *callee->get_args().begin();
	builder->create_ret(builder->create_imul(ConstantInt::get(2, module), &arg));

	auto main_fun = Function::create(FunctionType::get(int32, {}), "main", module);
	auto main_entry = BasicBlock::create(module, "entry", main_fun);
	builder->set_insert_point(main_entry);
	builder->create_ret(builder->create_call(callee, {ConstantInt::get(110, module)}));

	std::cout << module->print();
	delete module;
}