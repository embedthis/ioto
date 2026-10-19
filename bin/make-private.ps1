#!/usr/bin/env pwsh
#
#   make-private.ps1 - Make a directory private to the current user, LocalSystem and Administrators
#
#   The web server refuses to start with a state directory that anyone else can write, and a new
#   directory inherits its parent's entries: under a build workspace or a drive root those grant
#   write to Users. Replace the inherited entries with inheritable full control for the three
#   principals the server accepts, so everything created underneath is private too.
#
#   With -Check, change nothing: exit 0 if every access rule on the directory names one of those
#   principals, else name the first other principal and exit 1.
#

param(
    [Parameter(Mandatory = $true)][string]$Path,
    [switch]$Check
)

$ErrorActionPreference = "Stop"

$principals = @(
    [Security.Principal.WindowsIdentity]::GetCurrent().User,
    (New-Object Security.Principal.SecurityIdentifier "S-1-5-18"),
    (New-Object Security.Principal.SecurityIdentifier "S-1-5-32-544")
)

if ($Check) {
    $allowed = $principals | ForEach-Object { $_.Value }
    $rules = (Get-Acl -LiteralPath $Path).GetAccessRules($true, $true, [Security.Principal.SecurityIdentifier])
    foreach ($rule in $rules) {
        if ($allowed -notcontains $rule.IdentityReference.Value) {
            Write-Host "$Path grants $($rule.FileSystemRights) to $($rule.IdentityReference.Value)"
            exit 1
        }
    }
    exit 0
}

$acl = New-Object Security.AccessControl.DirectorySecurity
$acl.SetAccessRuleProtection($true, $false)
foreach ($sid in $principals) {
    $acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule($sid, "FullControl",
        "ContainerInherit,ObjectInherit", "None", "Allow")))
}
(Get-Item -LiteralPath $Path).SetAccessControl($acl)
