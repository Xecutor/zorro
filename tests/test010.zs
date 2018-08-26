class Base(z)
  z
  on create
    print("hello,z=",z)
  end
  on destroy
    print("ciao from base, z=",z)
  end
end

class Obj
  on create
    print("obj create")
  end
  on destroy
    print("obj destroy")
  end
end

saveMe=nil
saved=false

class Test(a,b):Base(b+1)
  x=a
  y=a+b
  o=Obj()
  on create
    y=y-1
    print("x=",x)
    print("y=",y)
    print("z=",z)
  end
  on destroy
    print("bye bye")
    if saved
      print("finally")
    else
      saveMe=self
      saved=true
    end
  end
  func test(a)
    x=x+a
    return x-y
  end
end


t=Test(1,2)
y=0.0
y+=t.test(3)
print(y)
t=nil
print("after")
saveMe=nil

class Simple(a_x,a_y)
  x=a_x
  y=a_y
end

s=Simple(10,20)
print(s.x," ",s.y)


class Child:Base(4)
end

c=Child()
c=nil
