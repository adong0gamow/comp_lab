define i32 @main() {
entry:
	%a = alloca i32
	%i = alloca i32
	store i32 10, i32* %a
	store i32 0, i32* %i
	br label %loop
loop:
	%iv = load i32, i32* %i
	%cond = icmp slt i32 %iv, 10
	br i1 %cond, label %body, label %end
body:
	%nexti = add i32 %iv, 1
	store i32 %nexti, i32* %i
	%av = load i32, i32* %a
	%sum = add i32 %av, %nexti
	store i32 %sum, i32* %a
	br label %loop
end:
	%result = load i32, i32* %a
	ret i32 %result
}
