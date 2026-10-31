define i32 @callee(i32 %a) {
entry:
	%v = mul i32 2, %a
	ret i32 %v
}

define i32 @main() {
entry:
	%result = call i32 @callee(i32 110)
	ret i32 %result
}
