//chained if/elsif
for i in 1..10
  if i==1
    print("one")
  elsif i==2
    print("two")
  elsif i==3
    print("three")
  elsif i==4
    print("four")
  else
    print("more")
  end
end

//string comparison
a="aaa"
b="bbb"
c="aaa"

print("ok") if a<b
print("ok") if a<=b
print("ok") if a<=c
print("ok") if b>a
print("ok") if b>=a
print("ok") if c>=a
print("ok") if a!=b
print("ok") if a==c

x=1.0
y=2.0
z=2.0
print("ok") if x<y
print("ok") if x<=y
print("ok") if x<=z
print("ok") if y>x
print("ok") if y>=x
print("ok") if z>=x
print("ok") if x==x
print("ok") if x!=y
