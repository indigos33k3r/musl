.set noreorder

.global _longjmp
.global longjmp
.type   _longjmp,@function
.type   longjmp,@function
_longjmp:
longjmp:
	move    $2, $5
	bne     $2, $0, 1f
	nop
	addu    $2, $2, 1
1:      lw      $ra,  0($4)
	lw      $sp,  4($4)
	lw      $16,  8($4)
	lw      $17, 12($4)
	lw      $18, 16($4)
	lw      $19, 20($4)
	lw      $20, 24($4)
	lw      $21, 28($4)
	lw      $22, 32($4)
	lw      $23, 36($4)
	lw      $30, 40($4)
	jr      $ra
	lw      $28, 44($4)
