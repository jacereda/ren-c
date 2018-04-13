; functions/math/add.r
(3 = add 1 2)
; integer -9223372036854775808 + x tests
<64bit>
(error? trap [add -9223372036854775808 -9223372036854775808])
<64bit>
(error? trap [add -9223372036854775808 -9223372036854775807])
<64bit>
(error? trap [add -9223372036854775808 -2147483648])
<64bit>
(error? trap [add -9223372036854775808 -1])
<64bit>
(-9223372036854775808 = add -9223372036854775808 0)
<64bit>
(-9223372036854775807 = add -9223372036854775808 1)
<64bit>
(-2 = add -9223372036854775808 9223372036854775806)
<64bit>
(-1 = add -9223372036854775808 9223372036854775807)
; integer -9223372036854775807 + x tests
<64bit>
(error? trap [add -9223372036854775807 -9223372036854775808])
<64bit>
(error? trap [add -9223372036854775807 -9223372036854775807])
<64bit>
(-9223372036854775808 = add -9223372036854775807 -1)
<64bit>
(-9223372036854775807 = add -9223372036854775807 0)
<64bit>
(-9223372036854775806 = add -9223372036854775807 1)
<64bit>
(-1 = add -9223372036854775807 9223372036854775806)
<64bit>
(0 = add -9223372036854775807 9223372036854775807)
; integer -2147483648 + x tests
<32bit>
(error? trap [add -2147483648 -2147483648])
<64bit>
(-4294967296 = add -2147483648 -2147483648)
<32bit>
(error? trap [add -2147483648 -1])
<64bit>
(-2147483649 = add -2147483648 -1)
(-2147483648 = add -2147483648 0)
(-2147483647 = add -2147483648 1)
(-1 = add -2147483648 2147483647)
; integer -1 + x tests
<64bit>
(error? trap [add -1 -9223372036854775808])
<64bit>
(-9223372036854775808 = add -1 -9223372036854775807)
(-2 = add -1 -1)
(-1 = add -1 0)
(0 = add -1 1)
<64bit>
(9223372036854775805 = add -1 9223372036854775806)
<64bit>
(9223372036854775806 = add -1 9223372036854775807)
; integer 0 + x tests
<64bit>
(-9223372036854775808 = add 0 -9223372036854775808)
<64bit>
(-9223372036854775807 = add 0 -9223372036854775807)
(-1 = add 0 -1)
[#28
    (0 = add 0 0)
]
(1 = add 0 1)
<64bit>
(9223372036854775806 = add 0 9223372036854775806)
<64bit>
(9223372036854775807 = add 0 9223372036854775807)
; integer 1 + x tests
<64bit>
(-9223372036854775807 = add 1 -9223372036854775808)
<64bit>
(-9223372036854775806 = add 1 -9223372036854775807)
(0 = add 1 -1)
(1 = add 1 0)
(2 = add 1 1)
<64bit>
(9223372036854775807 = add 1 9223372036854775806)
<64bit>
(error? trap [add 1 9223372036854775807])
; integer 2147483647 + x
(-1 = add 2147483647 -2147483648)
(2147483646 = add 2147483647 -1)
(2147483647 = add 2147483647 0)
<32bit>
(error? trap [add 2147483647 1])
<64bit>
(2147483648 = add 2147483647 1)
<32bit>
(error? trap [add 2147483647 2147483647])
<64bit>
(4294967294 = add 2147483647 2147483647)
; integer 9223372036854775806 + x tests
<64bit>
(-2 = add 9223372036854775806 -9223372036854775808)
<64bit>
(-1 = add 9223372036854775806 -9223372036854775807)
<64bit>
(9223372036854775805 = add 9223372036854775806 -1)
<64bit>
(9223372036854775806 = add 9223372036854775806 0)
<64bit>
(9223372036854775807 = add 9223372036854775806 1)
<64bit>
(error? trap [add 9223372036854775806 9223372036854775806])
<64bit>
(error? trap [add 9223372036854775806 9223372036854775807])
; integer 9223372036854775807 + x tests
<64bit>
(-1 = add 9223372036854775807 -9223372036854775808)
<64bit>
(0 = add 9223372036854775807 -9223372036854775807)
<64bit>
(9223372036854775806 = add 9223372036854775807 -1)
<64bit>
(9223372036854775807 = add 9223372036854775807 0)
<64bit>
(error? trap [add 9223372036854775807 1])
<64bit>
(error? trap [add 9223372036854775807 9223372036854775806])
<64bit>
(error? trap [add 9223372036854775807 9223372036854775807])
; decimal + integer
(2.1 = add 1.1 1)
(2147483648.0 = add 1.0 2147483647)
(-2147483649.0 = add -1.0 -2147483648)
; integer + decimal
(2.1 = add 1 1.1)
(2147483648.0 = add 2147483647 1.0)
(-2147483649.0 = add -2147483648 -1.0)
; -1.7976931348623157e308 + decimal
(error? trap [add -1.7976931348623157e308 -1.7976931348623157e308])
(-1.7976931348623157e308 = add -1.7976931348623157e308 -1.0)
(-1.7976931348623157e308 = add -1.7976931348623157e308 -4.94065645841247E-324)
(-1.7976931348623157e308 = add -1.7976931348623157e308 0.0)
(-1.7976931348623157e308 = add -1.7976931348623157e308 4.94065645841247E-324)
(-1.7976931348623157e308 = add -1.7976931348623157e308 1.0)
(0.0 = add -1.7976931348623157e308 1.7976931348623157e308)
; -1.0 + decimal
(-1.7976931348623157e308 = add -1.0 -1.7976931348623157e308)
(-2.0 = add -1.0 -1.0)
(-1.0 = add -1.0 -4.94065645841247E-324)
(-1.0 = add -1.0 0.0)
(-1.0 = add -1.0 4.94065645841247E-324)
(0.0 = add -1.0 1.0)
(1.7976931348623157e308 = add -1.0 1.7976931348623157e308)
; -4.94065645841247E-324 + decimal
(-1.7976931348623157e308 = add -4.94065645841247E-324 -1.7976931348623157e308)
(-1.0 = add -4.94065645841247E-324 -1.0)
(-9.88131291682493e-324 = add -4.94065645841247E-324 -4.94065645841247E-324)
(-4.94065645841247E-324 = add -4.94065645841247E-324 0.0)
(0.0 = add -4.94065645841247E-324 4.94065645841247E-324)
(1.0 = add -4.94065645841247E-324 1.0)
(1.7976931348623157e308 = add -4.94065645841247E-324 1.7976931348623157e308)
; 0.0 + decimal
(-1.7976931348623157e308 = add 0.0 -1.7976931348623157e308)
(-1.0 = add 0.0 -1.0)
(-4.94065645841247E-324 = add 0.0 -4.94065645841247E-324)
(0.0 = add 0.0 0.0)
(4.94065645841247E-324 = add 0.0 4.94065645841247E-324)
(1.0 = add 0.0 1.0)
(1.7976931348623157e308 = add 0.0 1.7976931348623157e308)
; 4.94065645841247E-324 + decimal
(-1.7976931348623157e308 = add 4.94065645841247E-324 -1.7976931348623157e308)
(-1.0 = add 4.94065645841247E-324 -1.0)
(0.0 = add 4.94065645841247E-324 -4.94065645841247E-324)
(4.94065645841247E-324 = add 4.94065645841247E-324 0.0)
(9.88131291682493e-324 = add 4.94065645841247E-324 4.94065645841247E-324)
(1.0 = add 4.94065645841247E-324 1.0)
(1.7976931348623157e308 = add 4.94065645841247E-324 1.7976931348623157e308)
; 1.0 + decimal
(-1.7976931348623157e308 = add 1.0 -1.7976931348623157e308)
(0.0 = add 1.0 -1.0)
(1.0 = add 1.0 4.94065645841247E-324)
(1.0 = add 1.0 0.0)
(1.0 = add 1.0 -4.94065645841247E-324)
(2.0 = add 1.0 1.0)
(1.7976931348623157e308 = add 1.0 1.7976931348623157e308)
; 1.7976931348623157e308 + decimal
(0.0 = add 1.7976931348623157e308 -1.7976931348623157e308)
(1.7976931348623157e308 = add 1.7976931348623157e308 -1.0)
(1.7976931348623157e308 = add 1.7976931348623157e308 -4.94065645841247E-324)
(1.7976931348623157e308 = add 1.7976931348623157e308 0.0)
(1.7976931348623157e308 = add 1.7976931348623157e308 4.94065645841247E-324)
(1.7976931348623157e308 = add 1.7976931348623157e308 1.0)
(error? trap [add 1.7976931348623157e308 1.7976931348623157e308])
; pair
(-2147483648x-2147483648 = add -2147483648x-2147483648 0x0)
(-2x-2 = add -1x-1 -1x-1)
(-1x-1 = add -1x-1 0x0)
(0x0 = add -1x-1 1x1)
(-2147483648x-2147483648 = add 0x0 -2147483648x-2147483648)
(-1x-1 = add 0x0 -1x-1)
(0x0 = add 0x0 0x0)
(1x1 = add 0x0 1x1)
(2147483647x2147483647 = add 0x0 2147483647x2147483647)
(0x0 = add 1x1 -1x-1)
(1x1 = add 1x1 0x0)
(2x2 = add 1x1 1x1)
(2147483647x2147483647 = add 2147483647x2147483647 0x0)
; pair + ...
(error? trap [0x0 + blank])
(error? trap [0x0 + ""])
; char
(#"^(00)" = add #"^(00)" #"^(00)")
(#"^(01)" = add #"^(00)" #"^(01)")
(#"^(ff)" = add #"^(00)" #"^(ff)")
(#"^(01)" = add #"^(01)" #"^(00)")
(#"^(02)" = add #"^(01)" #"^(01)")
(#"^(ff)" = add #"^(ff)" #"^(00)")
; tuple
(0.0.0 = add 0.0.0 0.0.0)
(0.0.1 = add 0.0.0 0.0.1)
(0.0.255 = add 0.0.0 0.0.255)
(0.0.1 = add 0.0.1 0.0.0)
(0.0.2 = add 0.0.1 0.0.1)
(0.0.255 = add 0.0.1 0.0.255)
(0.0.255 = add 0.0.255 0.0.0)
(0.0.255 = add 0.0.255 0.0.1)
(0.0.255 = add 0.0.255 0.0.255)

; Slipstream a test of ME in here, as the replacement for "++"
(
    some-var: 20
    some-var: me + 1 * 10
    some-var = 210
)
