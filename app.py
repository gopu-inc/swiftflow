
def sqrt(x):
   if x == 0:
      return 0;
   if x < 0:
      return 0;
   guess = x / 2.0;
      i = 1
   while i < 20: 
      guess = guess + x / guess / 2.0;
      i = i + 1
      return guess;

print(sqrt(5))
