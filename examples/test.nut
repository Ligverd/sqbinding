
my.name = "Hello"
my.i = 100

i88 <- my.func(44)
stest5 <- f_str_int("test",5)

local c = Test("Hello")
smulti1 <- c.multi(7)
smulti2 <- c.multi(8,"data")


Test.staticMethod(12)
TestStatic.static_prop = 567


test <- Test(44)
test << 6
opshiftl <- test.idx

test2 <- Test("text")
test2_single <- test2.single(42) // "idx: 55 name: text single(42)"

test3 <- Test(test2)
test3_single <- test3.single(43) // "idx: 55 name: text single(43)"

test4_isRef <- test.isRef(test2, test2)

c | " Ligverd"
opor <- c.name

c | " and" | " Lana"
oporor <- c.name

function customTypeFunction(t) {
  t.name += " " + "world"
  t.number += 1
  t.ar.append(9)

  return t
}

p <- TestP(345)
idx_TestP <- p.idx

arg_shared_ptr <- testArgPtr(p)


function modifyTestInSQ(t) {
  t.name = "sq_modified"
}