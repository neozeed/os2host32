echo === M28C4 standard-handle redirection regression ===
if exist m28c-one.txt del m28c-one.txt
if exist m28c-two.txt del m28c-two.txt
echo alpha > m28c-one.txt
echo beta >> m28c-one.txt
type m28c-one.txt
type m28c-one.txt > m28c-two.txt
echo --- copied through redirected TYPE ---
type m28c-two.txt
del m28c-one.txt
del m28c-two.txt
if exist m28c-one.txt echo M28C4_DELETE_ONE_FAILED
if exist m28c-two.txt echo M28C4_DELETE_TWO_FAILED
if not exist m28c-one.txt echo delete-one=OK
if not exist m28c-two.txt echo delete-two=OK
echo M28C4 redirection regression complete
