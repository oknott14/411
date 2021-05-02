	lw	1	0	op1	reg[1] <- op1
	lw	2	0	op2	reg[2] <- op2
	lw	3	0	op3	reg[3] <- op3
	add	4	1	3	stall & ID fetch correct r1
	srl	4	4	3	forward EX to EX
	addi	4	2	1	should not forward r4
	sub	5	4	3	choose correct val to forward
	add	5	5	4
	sw	5	0	40	alu -> sw regB
	lw	6	3	2	dataMem[1]
	sw	6	3	6	stall & store in data[2]
done	halt
op1	.fill	50
op2	.fill	30
op3	.fill	2
