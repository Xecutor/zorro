//test ref
x=1
y=&x
y=5
print(x)
x=10
print(y)

print("ok") if y==10
print("ok") if y!=5
print("ok") if y<11
print("ok") if y>9
print("ok") if y<=10
print("ok") if y>=10

//weak ref
a=[1,2,3]
b=&&a
print("$b")
a[0]=5
print("$b")
a=nil
print("$b")
