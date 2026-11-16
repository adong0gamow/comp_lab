define i32 @main() {
entry:
	%a = alloca float
	store float 0x40163851E0000000, float* %a
	%av = load float, float* %a
	%cond = fcmp ogt float %av, 1.000000e+00
	br i1 %cond, label %then, label %end
then:
	ret i32 233
end:
	ret i32 0
}
