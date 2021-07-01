#include <bits/stdc++.h>
#include "classifier.h"

using namespace std;

int main(void)
{
    Classifier c("/mnt/storage/takh/pgp/workloads/wordpress/first-pass/westmere-0-bbl-log.gz");
    c.find_all_context();
    return 0;
}