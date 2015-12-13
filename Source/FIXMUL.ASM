		PAGE	80,123

		.MODEL	LARGE,PASCAL

		.CODE

FixMul		PROC	USES si di, m1:DWORD, m2:DWORD
		mov	ax,WORD PTR (m1+2)
		mov	si,ax
		mov	dx,WORD PTR (m2+2)
		xor	si,dx				;save sign of result
		test	ax,ax
		jns	fix1				;not negative
		neg	ax
		neg	WORD PTR (m1)
		sbb	ax,0
		mov	WORD PTR (m1+2),ax
fix1:		test	dx,dx
		jns	fix2				;not negative
		neg	dx
		neg	WORD PTR (m2)
		sbb	dx,0
		mov	WORD PTR (m2+2),dx
fix2:		mul	dx
		test	dx,dx
		jnz	fix4				;overflow
		xchg	ax,cx
		mov	ax,WORD PTR (m1)
		mov	di,WORD PTR (m2)
		mul	di
		mov	bx,dx
		xchg	ax,di
		mul	WORD PTR (m1+2)
		shl	di,1				;round result
		adc	bx,ax
		adc	cx,dx
		jc	fix4				;overflow
		mov	ax,WORD PTR (m1)
		mul	WORD PTR (m2+2)
		add	ax,bx
		adc	dx,cx
		jc	fix4				;overflow
		js	fix4
		test	si,si
		jns	fix3				;result not negative
		neg	dx
		neg	ax
		sbb	dx,0
fix3:		ret
fix4:		xor	ax,ax
		mov	dx,8000h
		test	si,si
		js	fix3				;result is negative infinity
		dec	ax
		dec	dx
		ret
FixMul		ENDP

		END
