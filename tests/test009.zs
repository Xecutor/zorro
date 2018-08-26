//pre/post inc
x=1
x=x++
print(x)
y=++x
print(y," ",x)
x=x++ + ++x
print(x)
z=&y
z=++z + y++
print(z)
print(y)
