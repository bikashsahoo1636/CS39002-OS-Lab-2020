echo "Serial Random string" > 1c_input.txt
a=1
while IFS= read -r line
do
    echo "$a" $line >> 1c_input.txt
    let a=a+1
done < 1b_input.txt
echo "$a" $line >> 1c_input.txt