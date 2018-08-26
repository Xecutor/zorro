//chained if/elsif inside while
func f(x)
  y=0
  while y<2
    if x<100
      if x<10
        print(x,"<10")
      elsif x<50
        print(x,"<50")
      else
        print(x,">=50")
      end
    else
      print(x,">=100")
    end
    y=y+1
  end
  print("exit")
end
f(i) for i in 5..110@5
