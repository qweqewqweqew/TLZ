$ErrorActionPreference = 'Stop'

$sqlcmd = 'C:\Program Files\Microsoft SQL Server\Client SDK\ODBC\170\Tools\Binn\SQLCMD.EXE'
$server = 'localhost\SQLEXPRESS'
$targetLogin = "LAPTOP-C026B50D\l'ch"
$outFile = 'D:\QtProj\tongliiz\MzTLZ\downloads\sql_system_fix_output.txt'

$query = @"
IF NOT EXISTS (
    SELECT 1
    FROM sys.server_principals
    WHERE name = N'LAPTOP-C026B50D\l''ch'
)
BEGIN
    CREATE LOGIN [LAPTOP-C026B50D\l'ch] FROM WINDOWS;
END;

IF NOT EXISTS (
    SELECT 1
    FROM sys.server_role_members rm
    JOIN sys.server_principals role_p
        ON rm.role_principal_id = role_p.principal_id
    JOIN sys.server_principals member_p
        ON rm.member_principal_id = member_p.principal_id
    WHERE role_p.name = N'sysadmin'
      AND member_p.name = N'LAPTOP-C026B50D\l''ch'
)
BEGIN
    ALTER SERVER ROLE [sysadmin] ADD MEMBER [LAPTOP-C026B50D\l'ch];
END;

SELECT
    SYSTEM_USER AS [system_user],
    SUSER_SNAME() AS [suser_sname],
    CASE
        WHEN EXISTS (
            SELECT 1
            FROM sys.server_role_members rm
            JOIN sys.server_principals role_p
                ON rm.role_principal_id = role_p.principal_id
            JOIN sys.server_principals member_p
                ON rm.member_principal_id = member_p.principal_id
            WHERE role_p.name = N'sysadmin'
              AND member_p.name = N'LAPTOP-C026B50D\l''ch'
        )
        THEN 1
        ELSE 0
    END AS [target_is_sysadmin];
"@

& $sqlcmd -S $server -E -b -Q $query *> $outFile
