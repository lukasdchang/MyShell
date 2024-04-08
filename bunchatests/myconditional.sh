#!/bin/bash
echo Testing 'then' conditional:
true
then echo Success: This message should print because the previous command succeeded.

echo Testing 'else' conditional:
false
else echo Failure: This message should print because the previous command failed.

echo Testing 'else' without failure:
true
else echo Failure: This message should NOT print.

echo Testing 'then' without success:
false
then echo Success: This message should NOT print.

echo Attempting a command that succeeds:
true
then echo Success: This message should appear.

echo Attempting a command that fails:
false
then echo Success: This message should NOT appear.
else echo Failure: This message should appear.


# Expected output:
    # Testing 'then' conditional:
    # Success: This message should print because the previous command succeeded.

    # Testing 'else' conditional:
    # Failure: This message should print because the previous command failed.

    # Testing 'else' without failure:

    # Testing 'then' without success:

    # Attempting a command that succeeds:
    # Success: This message should appear.

    # Attempting a command that fails:
    # Failure: This message should appear.
