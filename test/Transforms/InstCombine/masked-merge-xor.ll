; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt < %s -instcombine -S | FileCheck %s

; https://bugs.llvm.org/show_bug.cgi?id=6773

; Patterns:
;   (x & m) | (y & ~m)
;   (x & m) ^ (y & ~m)
;   (x & m) + (y & ~m)
; Should be transformed into:
;   (x & m) | (y & ~m)
; And then into:
;   ((x ^ y) & m) ^ y

; ============================================================================ ;
; Most basic positive tests
; ============================================================================ ;

define i32 @p(i32 %x, i32 %y, i32 %m) {
; CHECK-LABEL: @p(
; CHECK-NEXT:    [[TMP1:%.*]] = xor i32 [[X:%.*]], [[Y:%.*]]
; CHECK-NEXT:    [[TMP2:%.*]] = and i32 [[TMP1]], [[M:%.*]]
; CHECK-NEXT:    [[RET:%.*]] = xor i32 [[TMP2]], [[Y]]
; CHECK-NEXT:    ret i32 [[RET]]
;
  %and = and i32 %x, %m
  %neg = xor i32 %m, -1
  %and1 = and i32 %neg, %y
  %ret = xor i32 %and, %and1
  ret i32 %ret
}

define <2 x i32> @p_splatvec(<2 x i32> %x, <2 x i32> %y, <2 x i32> %m) {
; CHECK-LABEL: @p_splatvec(
; CHECK-NEXT:    [[TMP1:%.*]] = xor <2 x i32> [[X:%.*]], [[Y:%.*]]
; CHECK-NEXT:    [[TMP2:%.*]] = and <2 x i32> [[TMP1]], [[M:%.*]]
; CHECK-NEXT:    [[RET:%.*]] = xor <2 x i32> [[TMP2]], [[Y]]
; CHECK-NEXT:    ret <2 x i32> [[RET]]
;
  %and = and <2 x i32> %x, %m
  %neg = xor <2 x i32> %m, <i32 -1, i32 -1>
  %and1 = and <2 x i32> %neg, %y
  %ret = xor <2 x i32> %and, %and1
  ret <2 x i32> %ret
}

define <3 x i32> @p_vec_undef(<3 x i32> %x, <3 x i32> %y, <3 x i32> %m) {
; CHECK-LABEL: @p_vec_undef(
; CHECK-NEXT:    [[TMP1:%.*]] = xor <3 x i32> [[X:%.*]], [[Y:%.*]]
; CHECK-NEXT:    [[TMP2:%.*]] = and <3 x i32> [[TMP1]], [[M:%.*]]
; CHECK-NEXT:    [[RET:%.*]] = xor <3 x i32> [[TMP2]], [[Y]]
; CHECK-NEXT:    ret <3 x i32> [[RET]]
;
  %and = and <3 x i32> %x, %m
  %neg = xor <3 x i32> %m, <i32 -1, i32 undef, i32 -1>
  %and1 = and <3 x i32> %neg, %y
  %ret = xor <3 x i32> %and, %and1
  ret <3 x i32> %ret
}

; ============================================================================ ;
; Constant mask.
; ============================================================================ ;

define i32 @p_constmask(i32 %x, i32 %y) {
; CHECK-LABEL: @p_constmask(
; CHECK-NEXT:    [[AND:%.*]] = and i32 [[X:%.*]], 65280
; CHECK-NEXT:    [[AND1:%.*]] = and i32 [[Y:%.*]], -65281
; CHECK-NEXT:    [[RET1:%.*]] = or i32 [[AND]], [[AND1]]
; CHECK-NEXT:    ret i32 [[RET1]]
;
  %and = and i32 %x, 65280
  %and1 = and i32 %y, -65281
  %ret = xor i32 %and, %and1
  ret i32 %ret
}

define <2 x i32> @p_constmask_splatvec(<2 x i32> %x, <2 x i32> %y) {
; CHECK-LABEL: @p_constmask_splatvec(
; CHECK-NEXT:    [[AND:%.*]] = and <2 x i32> [[X:%.*]], <i32 65280, i32 65280>
; CHECK-NEXT:    [[AND1:%.*]] = and <2 x i32> [[Y:%.*]], <i32 -65281, i32 -65281>
; CHECK-NEXT:    [[RET1:%.*]] = or <2 x i32> [[AND]], [[AND1]]
; CHECK-NEXT:    ret <2 x i32> [[RET1]]
;
  %and = and <2 x i32> %x, <i32 65280, i32 65280>
  %and1 = and <2 x i32> %y, <i32 -65281, i32 -65281>
  %ret = xor <2 x i32> %and, %and1
  ret <2 x i32> %ret
}

define <2 x i32> @p_constmask_vec(<2 x i32> %x, <2 x i32> %y) {
; CHECK-LABEL: @p_constmask_vec(
; CHECK-NEXT:    [[AND:%.*]] = and <2 x i32> [[X:%.*]], <i32 65280, i32 16776960>
; CHECK-NEXT:    [[AND1:%.*]] = and <2 x i32> [[Y:%.*]], <i32 -65281, i32 -16776961>
; CHECK-NEXT:    [[RET:%.*]] = xor <2 x i32> [[AND]], [[AND1]]
; CHECK-NEXT:    ret <2 x i32> [[RET]]
;
  %and = and <2 x i32> %x, <i32 65280, i32 16776960>
  %and1 = and <2 x i32> %y, <i32 -65281, i32 -16776961>
  %ret = xor <2 x i32> %and, %and1
  ret <2 x i32> %ret
}

define <3 x i32> @p_constmask_vec_undef(<3 x i32> %x, <3 x i32> %y) {
; CHECK-LABEL: @p_constmask_vec_undef(
; CHECK-NEXT:    [[AND:%.*]] = and <3 x i32> [[X:%.*]], <i32 65280, i32 undef, i32 65280>
; CHECK-NEXT:    [[AND1:%.*]] = and <3 x i32> [[Y:%.*]], <i32 -65281, i32 undef, i32 -65281>
; CHECK-NEXT:    [[RET:%.*]] = xor <3 x i32> [[AND]], [[AND1]]
; CHECK-NEXT:    ret <3 x i32> [[RET]]
;
  %and = and <3 x i32> %x, <i32 65280, i32 undef, i32 65280>
  %and1 = and <3 x i32> %y, <i32 -65281, i32 undef, i32 -65281>
  %ret = xor <3 x i32> %and, %and1
  ret <3 x i32> %ret
}

; ============================================================================ ;
; Constant mask with no common bits set, but common unset bits.
; ============================================================================ ;

define i32 @p_constmask2(i32 %x, i32 %y) {
; CHECK-LABEL: @p_constmask2(
; CHECK-NEXT:    [[AND:%.*]] = and i32 [[X:%.*]], 61440
; CHECK-NEXT:    [[AND1:%.*]] = and i32 [[Y:%.*]], -65281
; CHECK-NEXT:    [[RET1:%.*]] = or i32 [[AND]], [[AND1]]
; CHECK-NEXT:    ret i32 [[RET1]]
;
  %and = and i32 %x, 61440
  %and1 = and i32 %y, -65281
  %ret = xor i32 %and, %and1
  ret i32 %ret
}

define <2 x i32> @p_constmask2_splatvec(<2 x i32> %x, <2 x i32> %y) {
; CHECK-LABEL: @p_constmask2_splatvec(
; CHECK-NEXT:    [[AND:%.*]] = and <2 x i32> [[X:%.*]], <i32 61440, i32 61440>
; CHECK-NEXT:    [[AND1:%.*]] = and <2 x i32> [[Y:%.*]], <i32 -65281, i32 -65281>
; CHECK-NEXT:    [[RET1:%.*]] = or <2 x i32> [[AND]], [[AND1]]
; CHECK-NEXT:    ret <2 x i32> [[RET1]]
;
  %and = and <2 x i32> %x, <i32 61440, i32 61440>
  %and1 = and <2 x i32> %y, <i32 -65281, i32 -65281>
  %ret = xor <2 x i32> %and, %and1
  ret <2 x i32> %ret
}

define <2 x i32> @p_constmask2_vec(<2 x i32> %x, <2 x i32> %y) {
; CHECK-LABEL: @p_constmask2_vec(
; CHECK-NEXT:    [[AND:%.*]] = and <2 x i32> [[X:%.*]], <i32 61440, i32 16711680>
; CHECK-NEXT:    [[AND1:%.*]] = and <2 x i32> [[Y:%.*]], <i32 -65281, i32 -16776961>
; CHECK-NEXT:    [[RET:%.*]] = xor <2 x i32> [[AND]], [[AND1]]
; CHECK-NEXT:    ret <2 x i32> [[RET]]
;
  %and = and <2 x i32> %x, <i32 61440, i32 16711680>
  %and1 = and <2 x i32> %y, <i32 -65281, i32 -16776961>
  %ret = xor <2 x i32> %and, %and1
  ret <2 x i32> %ret
}

define <3 x i32> @p_constmask2_vec_undef(<3 x i32> %x, <3 x i32> %y) {
; CHECK-LABEL: @p_constmask2_vec_undef(
; CHECK-NEXT:    [[AND:%.*]] = and <3 x i32> [[X:%.*]], <i32 61440, i32 undef, i32 61440>
; CHECK-NEXT:    [[AND1:%.*]] = and <3 x i32> [[Y:%.*]], <i32 -65281, i32 undef, i32 -65281>
; CHECK-NEXT:    [[RET:%.*]] = xor <3 x i32> [[AND]], [[AND1]]
; CHECK-NEXT:    ret <3 x i32> [[RET]]
;
  %and = and <3 x i32> %x, <i32 61440, i32 undef, i32 61440>
  %and1 = and <3 x i32> %y, <i32 -65281, i32 undef, i32 -65281>
  %ret = xor <3 x i32> %and, %and1
  ret <3 x i32> %ret
}

; ============================================================================ ;
; Commutativity.
; ============================================================================ ;

; Used to make sure that the IR complexity sorting does not interfere.
declare i32 @gen32()

define i32 @p_commutative0(i32 %x, i32 %y, i32 %m) {
; CHECK-LABEL: @p_commutative0(
; CHECK-NEXT:    [[TMP1:%.*]] = xor i32 [[X:%.*]], [[Y:%.*]]
; CHECK-NEXT:    [[TMP2:%.*]] = and i32 [[TMP1]], [[M:%.*]]
; CHECK-NEXT:    [[RET:%.*]] = xor i32 [[TMP2]], [[Y]]
; CHECK-NEXT:    ret i32 [[RET]]
;
  %and = and i32 %m, %x ; swapped order
  %neg = xor i32 %m, -1
  %and1 = and i32 %neg, %y
  %ret = xor i32 %and, %and1
  ret i32 %ret
}

define i32 @p_commutative1(i32 %x, i32 %m) {
; CHECK-LABEL: @p_commutative1(
; CHECK-NEXT:    [[Y:%.*]] = call i32 @gen32()
; CHECK-NEXT:    [[TMP1:%.*]] = xor i32 [[Y]], [[X:%.*]]
; CHECK-NEXT:    [[TMP2:%.*]] = and i32 [[TMP1]], [[M:%.*]]
; CHECK-NEXT:    [[RET:%.*]] = xor i32 [[TMP2]], [[Y]]
; CHECK-NEXT:    ret i32 [[RET]]
;
  %y = call i32 @gen32()
  %and = and i32 %x, %m
  %neg = xor i32 %m, -1
  %and1 = and i32 %y, %neg; swapped order
  %ret = xor i32 %and, %and1
  ret i32 %ret
}

define i32 @p_commutative2(i32 %x, i32 %y, i32 %m) {
; CHECK-LABEL: @p_commutative2(
; CHECK-NEXT:    [[TMP1:%.*]] = xor i32 [[X:%.*]], [[Y:%.*]]
; CHECK-NEXT:    [[TMP2:%.*]] = and i32 [[TMP1]], [[M:%.*]]
; CHECK-NEXT:    [[RET:%.*]] = xor i32 [[TMP2]], [[Y]]
; CHECK-NEXT:    ret i32 [[RET]]
;
  %and = and i32 %x, %m
  %neg = xor i32 %m, -1
  %and1 = and i32 %neg, %y
  %ret = xor i32 %and1, %and ; swapped order
  ret i32 %ret
}

define i32 @p_commutative3(i32 %x, i32 %m) {
; CHECK-LABEL: @p_commutative3(
; CHECK-NEXT:    [[Y:%.*]] = call i32 @gen32()
; CHECK-NEXT:    [[TMP1:%.*]] = xor i32 [[Y]], [[X:%.*]]
; CHECK-NEXT:    [[TMP2:%.*]] = and i32 [[TMP1]], [[M:%.*]]
; CHECK-NEXT:    [[RET:%.*]] = xor i32 [[TMP2]], [[Y]]
; CHECK-NEXT:    ret i32 [[RET]]
;
  %y = call i32 @gen32()
  %and = and i32 %m, %x ; swapped order
  %neg = xor i32 %m, -1
  %and1 = and i32 %y, %neg; swapped order
  %ret = xor i32 %and, %and1
  ret i32 %ret
}

define i32 @p_commutative4(i32 %x, i32 %y, i32 %m) {
; CHECK-LABEL: @p_commutative4(
; CHECK-NEXT:    [[TMP1:%.*]] = xor i32 [[X:%.*]], [[Y:%.*]]
; CHECK-NEXT:    [[TMP2:%.*]] = and i32 [[TMP1]], [[M:%.*]]
; CHECK-NEXT:    [[RET:%.*]] = xor i32 [[TMP2]], [[Y]]
; CHECK-NEXT:    ret i32 [[RET]]
;
  %and = and i32 %m, %x ; swapped order
  %neg = xor i32 %m, -1
  %and1 = and i32 %neg, %y
  %ret = xor i32 %and1, %and ; swapped order
  ret i32 %ret
}

define i32 @p_commutative5(i32 %x, i32 %m) {
; CHECK-LABEL: @p_commutative5(
; CHECK-NEXT:    [[Y:%.*]] = call i32 @gen32()
; CHECK-NEXT:    [[TMP1:%.*]] = xor i32 [[Y]], [[X:%.*]]
; CHECK-NEXT:    [[TMP2:%.*]] = and i32 [[TMP1]], [[M:%.*]]
; CHECK-NEXT:    [[RET:%.*]] = xor i32 [[TMP2]], [[Y]]
; CHECK-NEXT:    ret i32 [[RET]]
;
  %y = call i32 @gen32()
  %and = and i32 %x, %m
  %neg = xor i32 %m, -1
  %and1 = and i32 %y, %neg; swapped order
  %ret = xor i32 %and1, %and ; swapped order
  ret i32 %ret
}

define i32 @p_commutative6(i32 %x, i32 %m) {
; CHECK-LABEL: @p_commutative6(
; CHECK-NEXT:    [[Y:%.*]] = call i32 @gen32()
; CHECK-NEXT:    [[TMP1:%.*]] = xor i32 [[Y]], [[X:%.*]]
; CHECK-NEXT:    [[TMP2:%.*]] = and i32 [[TMP1]], [[M:%.*]]
; CHECK-NEXT:    [[RET:%.*]] = xor i32 [[TMP2]], [[Y]]
; CHECK-NEXT:    ret i32 [[RET]]
;
  %y = call i32 @gen32()
  %and = and i32 %m, %x ; swapped order
  %neg = xor i32 %m, -1
  %and1 = and i32 %y, %neg; swapped order
  %ret = xor i32 %and1, %and ; swapped order
  ret i32 %ret
}

define i32 @p_constmask_commutative(i32 %x, i32 %y) {
; CHECK-LABEL: @p_constmask_commutative(
; CHECK-NEXT:    [[AND:%.*]] = and i32 [[X:%.*]], 65280
; CHECK-NEXT:    [[AND1:%.*]] = and i32 [[Y:%.*]], -65281
; CHECK-NEXT:    [[RET1:%.*]] = or i32 [[AND1]], [[AND]]
; CHECK-NEXT:    ret i32 [[RET1]]
;
  %and = and i32 %x, 65280
  %and1 = and i32 %y, -65281
  %ret = xor i32 %and1, %and ; swapped order
  ret i32 %ret
}

; ============================================================================ ;
; Negative tests. Should not be folded.
; ============================================================================ ;

; One use only.

declare void @use32(i32)

define i32 @n0_oneuse(i32 %x, i32 %y, i32 %m) {
; CHECK-LABEL: @n0_oneuse(
; CHECK-NEXT:    [[AND:%.*]] = and i32 [[X:%.*]], [[M:%.*]]
; CHECK-NEXT:    [[NEG:%.*]] = xor i32 [[M]], -1
; CHECK-NEXT:    [[AND1:%.*]] = and i32 [[NEG]], [[Y:%.*]]
; CHECK-NEXT:    [[RET:%.*]] = or i32 [[AND]], [[AND1]]
; CHECK-NEXT:    call void @use32(i32 [[AND]])
; CHECK-NEXT:    call void @use32(i32 [[NEG]])
; CHECK-NEXT:    call void @use32(i32 [[AND1]])
; CHECK-NEXT:    ret i32 [[RET]]
;
  %and = and i32 %x, %m
  %neg = xor i32 %m, -1
  %and1 = and i32 %neg, %y
  %ret = xor i32 %and, %and1
  call void @use32(i32 %and)
  call void @use32(i32 %neg)
  call void @use32(i32 %and1)
  ret i32 %ret
}

define i32 @n0_constmask_oneuse(i32 %x, i32 %y) {
; CHECK-LABEL: @n0_constmask_oneuse(
; CHECK-NEXT:    [[AND:%.*]] = and i32 [[X:%.*]], 65280
; CHECK-NEXT:    [[AND1:%.*]] = and i32 [[Y:%.*]], -65281
; CHECK-NEXT:    [[RET1:%.*]] = or i32 [[AND]], [[AND1]]
; CHECK-NEXT:    call void @use32(i32 [[AND]])
; CHECK-NEXT:    call void @use32(i32 [[AND1]])
; CHECK-NEXT:    ret i32 [[RET1]]
;
  %and = and i32 %x, 65280
  %and1 = and i32 %y, -65281
  %ret = xor i32 %and, %and1
  call void @use32(i32 %and)
  call void @use32(i32 %and1)
  ret i32 %ret
}

; Bad xor constant

define i32 @n1_badxor(i32 %x, i32 %y, i32 %m) {
; CHECK-LABEL: @n1_badxor(
; CHECK-NEXT:    [[AND:%.*]] = and i32 [[X:%.*]], [[M:%.*]]
; CHECK-NEXT:    [[NEG:%.*]] = xor i32 [[M]], 1
; CHECK-NEXT:    [[AND1:%.*]] = and i32 [[NEG]], [[Y:%.*]]
; CHECK-NEXT:    [[RET:%.*]] = xor i32 [[AND]], [[AND1]]
; CHECK-NEXT:    ret i32 [[RET]]
;
  %and = and i32 %x, %m
  %neg = xor i32 %m, 1 ; not -1
  %and1 = and i32 %neg, %y
  %ret = xor i32 %and, %and1
  ret i32 %ret
}

; Different mask is used

define i32 @n2_badmask(i32 %x, i32 %y, i32 %m1, i32 %m2) {
; CHECK-LABEL: @n2_badmask(
; CHECK-NEXT:    [[AND:%.*]] = and i32 [[M1:%.*]], [[X:%.*]]
; CHECK-NEXT:    [[NEG:%.*]] = xor i32 [[M2:%.*]], -1
; CHECK-NEXT:    [[AND1:%.*]] = and i32 [[NEG]], [[Y:%.*]]
; CHECK-NEXT:    [[RET:%.*]] = xor i32 [[AND]], [[AND1]]
; CHECK-NEXT:    ret i32 [[RET]]
;
  %and = and i32 %m1, %x
  %neg = xor i32 %m2, -1 ; different mask, not %m1
  %and1 = and i32 %neg, %y
  %ret = xor i32 %and, %and1
  ret i32 %ret
}

; Different const mask is used

define i32 @n3_constmask_badmask(i32 %x, i32 %y) {
; CHECK-LABEL: @n3_constmask_badmask(
; CHECK-NEXT:    [[AND:%.*]] = and i32 [[X:%.*]], 65280
; CHECK-NEXT:    [[AND1:%.*]] = and i32 [[Y:%.*]], -65280
; CHECK-NEXT:    [[RET:%.*]] = xor i32 [[AND]], [[AND1]]
; CHECK-NEXT:    ret i32 [[RET]]
;
  %and = and i32 %x, 65280
  %and1 = and i32 %y, -65280 ; not -65281, so they have one common bit
  %ret = xor i32 %and, %and1
  ret i32 %ret
}

define i32 @n3_constmask_samemask(i32 %x, i32 %y) {
; CHECK-LABEL: @n3_constmask_samemask(
; CHECK-NEXT:    [[AND2:%.*]] = xor i32 [[X:%.*]], [[Y:%.*]]
; CHECK-NEXT:    [[RET:%.*]] = and i32 [[AND2]], 65280
; CHECK-NEXT:    ret i32 [[RET]]
;
  %and = and i32 %x, 65280
  %and1 = and i32 %y, 65280 ; both masks are the same
  %ret = xor i32 %and, %and1
  ret i32 %ret
}
