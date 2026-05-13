print("=== Demo ===")

local collect = Collect()

local c1 = Cat()
local c2 = Cat()

local b1 = Bird()
local b2 = Bird()

print("=== Call methods ===")

print( c1.jump() )
print( b1.fly() )

print("=== Push class as base class ===")

collect.push(c1)
collect.push(c2)
collect.push(b1)
collect.push(b2)
print("ok")

print("=== Call base method and read chail property ===")

collect.show()




