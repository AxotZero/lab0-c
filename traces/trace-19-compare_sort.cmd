# Test of insert_head and remove_head
option fail 0
option malloc 0

new
ih axot 250000
ih cccc 250000
ih emma 250000

# sort sorted array
time sort
time linux_sort

# sort reversed sorted array
reverse
time sort
reverse
time linux_sort

# sort shuffled array
shuffle_dp
time sort
shuffle_dp
time linux_sort
