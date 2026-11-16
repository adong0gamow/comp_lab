define i32 @main() {
entry:
	%a = alloca [10 x i32]
	%a0 = getelementptr [10 x i32], [10 x i32]* %a, i32 0, i32 0
	store i32 10, i32* %a0
	%a1 = getelementptr [10 x i32], [10 x i32]* %a, i32 0, i32 1
	%v0 = load i32, i32* %a0
	%v1 = mul i32 %v0, 2
	store i32 %v1, i32* %a1
	%result = load i32, i32* %a1
	ret i32 %result
}
