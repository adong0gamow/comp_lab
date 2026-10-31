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
	auto loop = BasicBlock::create(module, "loop", main_fun);
	auto body = BasicBlock::create(module, "body", main_fun);
	auto end = BasicBlock::create(module, "end", main_fun);
	builder->set_insert_point(entry);
	auto a = builder->create_alloca(int32);
	auto i = builder->create_alloca(int32);
	builder->create_store(ConstantInt::get(10, module), a);
	builder->create_store(ConstantInt::get(0, module), i);
	builder->create_br(loop);
	builder->set_insert_point(loop);
	auto iv = builder->create_load(i);
	auto condition = builder->create_icmp_lt(iv, ConstantInt::get(10, module));
	builder->create_cond_br(condition, body, end);
	builder->set_insert_point(body);
	auto next_i = builder->create_iadd(iv, ConstantInt::get(1, module));
	builder->create_store(next_i, i);
	auto sum = builder->create_iadd(builder->create_load(a), next_i);
	builder->create_store(sum, a);
	builder->create_br(loop);
	builder->set_insert_point(end);
	builder->create_ret(builder->create_load(a));

	std::cout << module->print();
	delete module;
}
