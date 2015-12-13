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

FixDiv		PROC	USES si, num:DWORD, den:DWORD
		mov	ax,WORD PTR (den+2)
		mov	si,ax
		mov	dx,WORD PTR (num+2)
		xor	si,dx				;save sign of result
		test	ax,ax
		jns	fix1				;not negative
		neg	ax
		neg	WORD PTR (den)
		sbb	ax,0
		mov	WORD PTR (den+2),ax
fix1:		test	dx,dx
		jns	fix2				;not negative
		neg	dx
		neg	WORD PTR (num)
		sbb	dx,0
		mov	WORD PTR (num+2),dx
fix2:		mov	bx,WORD PTR (den)
		test	ax,ax
		jnz	fix3				;not dividing by frac only
		mov	cx,ax
		cmp	bx,dx
		ja	fix4				;not an overflow
fix5:		xor	ax,ax
		mov	dx,8000h
		test	si,si
		js	fix6				;negative infinity
		dec	ax
		dec	dx
fix6:		ret
fix4:		mov	ax,WORD PTR (num)
fix7:		div	bx
		xchg	ax,cx				;save int part
		div	bx
		shl	dx,1
		cmp	dx,bx
		mov	dx,cx				;get in dx:ax
		cmc					;round result
		adc	ax,0
		adc	dx,0
		jc	fix5
		js	fix5				;overflow
fix12:		test	si,si
		jns	fix8				;result not negative
		neg	dx
		neg	ax
		sbb	dx,0
fix8:		ret
fix3:		test	bx,bx
		jnz	fix9				;not dividing by int only
		xchg	ax,bx
		xchg	ax,dx
		mov	cx,WORD PTR (num)
		jmp	fix7
fix9:		push	si				;do the divide the hard way
		push	di
		xor	si,si
		mov	di,dx
		mov	dx,WORD PTR (num)
		push	bp
		xchg	ax,bp
		mov	ax,si
		mov	cx,32
fix10:		stc					;main divide loop
		rcl	ax,1
		rcl	dx,1
		rcl	di,1
		rcl	si,1
		sub	di,bx
		sbb	si,bp
		jnb	fix11
		dec	ax
		add	di,bx
		adc	si,bp
fix11:		loop	fix10
		shl	di,1				;round result
		rcl	si,1
		sub	di,bx
		sbb	si,bp
		pop	bp
		pop	di
		pop	si
		jb	fix12				;don't round
		add	ax,1
		adc	dx,0
		jmp	fix12
FixDiv		ENDP

		END
