#!/usr/bin/env php
<?php

header('Content-Type: application/json');

try {
    // Parse the query string
    $query = $_SERVER['QUERY_STRING'];
    parse_str($query, $params);

    // Get multiplicands
    if (!isset($params['multiplicand1']) || !isset($params['multiplicand2'])) {
        throw new Exception("Invalid or missing parameters");
    }

    $multiplicand1 = floatval($params['multiplicand1']);
    $multiplicand2 = floatval($params['multiplicand2']);

    // Perform multiplication
    $result = $multiplicand1 * $multiplicand2;

    // Create response
    $response = [
        "result" => $result
    ];
    echo json_encode($response);
} catch (Exception $e) {
    // Handle errors in input
    $response = [
        "error" => $e->getMessage()
    ];
    echo json_encode($response);
}
?>

