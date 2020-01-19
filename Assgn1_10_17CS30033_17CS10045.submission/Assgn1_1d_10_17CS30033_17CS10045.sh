mkdir 1.d.files.out
read_dir="./1.d.files"
out_dir="./1.d.files.out"  

for filename in ./1.d.files/*; do
    name=${filename##*/}
    echo $name
    outfile=$out_dir/$name
    sort -r -n -k 1 $filename > $outfile
done

cd $out_dir

cat *.txt > "temp.txt"
sort -r -n -k 1 "temp.txt" > "1.d.out.txt"
rm "temp.txt"