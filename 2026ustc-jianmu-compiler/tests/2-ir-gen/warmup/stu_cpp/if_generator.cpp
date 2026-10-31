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
	auto float32 = module->get_float_type();
	auto main_fun = Function::create(FunctionType::get(int32, {}), "main", module);
	auto entry = BasicBlock::create(module, "entry", main_fun);
	auto then_bb = BasicBlock::create(module, "then", main_fun);
	auto end_bb = BasicBlock::create(module, "end", main_fun);
	builder->set_insert_point(entry);
	auto a = builder->create_alloca(float32);
	builder->create_store(ConstantFP::get(5.555f, module), a);
	auto condition = builder->create_fcmp_gt(builder->create_load(a), ConstantFP::get(1.0f, module));
	builder->create_cond_br(condition, then_bb, end_bb);
	builder->set_insert_point(then_bb);
	builder->create_ret(ConstantInt::get(233, module));
	builder->set_insert_point(end_bb);
	builder->create_ret(ConstantInt::get(0, module));

	std::cout << module->print();
	delete module;
}