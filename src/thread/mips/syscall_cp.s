.set    noreorder

.global __syscall_cp_asm
.type   __syscall_cp_asm,@function
__syscall_cp_asm:
.global __cp_begin
__cp_begin:
	lw      $4, 0($4)
	bne     $4, $0, 2f
	move    $25, $5
	move    $4, $6
	move    $5, $7
	lw      $6, 16($sp)
	lw      $7, 20($sp)
	lw      $8, 24($sp)
	lw      $9, 28($sp)
	subu    $sp, $sp, 32
	sw      $8, 16($sp)
	sw      $9, 20($sp)
	move    $2, $25
	syscall
.global __cp_end
__cp_end:
	beq     $7, $0, 1f
	addu    $sp, $sp, 32
	subu    $2, $0, $2
1:	jr      $ra
	nop
2:	lw      $25, %call16(__cancel)($gp)
	jr      $25
	nop
