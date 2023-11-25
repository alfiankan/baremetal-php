--TEST--
PDO ODBC auto commit mode
--EXTENSIONS--
pdo_odbc
--SKIPIF--
<?php
require 'ext/pdo/tests/pdo_test.inc';
PDOTest::skip();
?>
--FILE--
<?php
require 'ext/pdo/tests/pdo_test.inc';
$db = PDOTest::test_factory(dirname(__FILE__) . '/common.phpt');
$db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

echo "========== not in transaction ==========\n";

echo "auto commit ON from ON\n";
$db->setAttribute(PDO::ATTR_AUTOCOMMIT, true);
echo "Success\n\n";

echo "auto commit OFF from ON\n";
$db->setAttribute(PDO::ATTR_AUTOCOMMIT, false);
echo "Success\n\n";

echo "auto commit OFF from OFF\n";
$db->setAttribute(PDO::ATTR_AUTOCOMMIT, false);
echo "Success\n\n";

echo "auto commit ON from OFF\n";
$db->setAttribute(PDO::ATTR_AUTOCOMMIT, true);
echo "Success\n\n";

echo "========== in transaction ==========\n";

echo "begin transaction\n";
$db->beginTransaction();
echo "\n";

echo "auto commit ON from ON, expect error\n";
try {
    $db->setAttribute(PDO::ATTR_AUTOCOMMIT, true);
} catch (PDOException $e) {
    echo $e->getMessage()."\n\n";
}

echo "auto commit OFF from ON, expect error\n";
try {
    $db->setAttribute(PDO::ATTR_AUTOCOMMIT, false);
} catch (PDOException $e) {
    echo $e->getMessage()."\n\n";
}

echo "end transaction\n";
$db->rollback();

echo "auto commit OFF\n";
$db->setAttribute(PDO::ATTR_AUTOCOMMIT, false);

echo "begin transaction\n";
$db->beginTransaction();
echo "\n";

echo "auto commit ON from OFF, expect error\n";
try {
    $db->setAttribute(PDO::ATTR_AUTOCOMMIT, true);
} catch (PDOException $e) {
    echo $e->getMessage()."\n\n";
}

echo "auto commit OFF from OFF, expect error\n";
try {
    $db->setAttribute(PDO::ATTR_AUTOCOMMIT, false);
} catch (PDOException $e) {
    echo $e->getMessage()."\n\n";
}

echo "end transaction\n";
$db->rollback();
echo "\n";

echo "done!";
?>
--EXPECT--
========== not in transaction ==========
auto commit ON from ON
Success

auto commit OFF from ON
Success

auto commit OFF from OFF
Success

auto commit ON from OFF
Success

========== in transaction ==========
begin transaction

auto commit ON from ON, expect error
SQLSTATE[HY000]: General error: Cannot change autocommit mode while a transaction is already open

auto commit OFF from ON, expect error
SQLSTATE[HY000]: General error: Cannot change autocommit mode while a transaction is already open

end transaction
auto commit OFF
begin transaction

auto commit ON from OFF, expect error
SQLSTATE[HY000]: General error: Cannot change autocommit mode while a transaction is already open

auto commit OFF from OFF, expect error
SQLSTATE[HY000]: General error: Cannot change autocommit mode while a transaction is already open

end transaction

done!
