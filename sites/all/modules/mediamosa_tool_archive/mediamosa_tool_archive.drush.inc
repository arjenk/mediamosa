<?php
/**
 * @file
 * Drush integration for MediaMosa Tool Archive.
 */

/**
 * Implements hook_drush_command().
 */
function mediamosa_tool_archive_drush_command() {
  $items = array();

  $items['mm-archive'] = array(
    'description' => dt('Create a archive test job -- test.'),
  );

  return $items;
}

/**
 * Callback function for mm-archive command.
 *
 * Created test job for tool archive.
 */
function drush_mediamosa_tool_archive_mm_archive($asset_id) {

  // Parameters for assets/download.
  $parameters = array(
    mediamosa_rest_call_archive_create::USER_ID => 'foo',
    mediamosa_rest_call_archive_create::ASSET_ID . '[]' => $asset_id,
    mediamosa_rest_call_archive_create::PROFILE => 0,
    mediamosa_rest_call_archive_create::FILENAME => 'foo.zip',
    mediamosa_rest_call_archive_create::COMPLETED_TRANSCODING_URL => 'http://localhost/done',
  );
  $response = mediamosa_ck::request_get('archive/create', array('data' => $parameters, 'method' => 'GET'));
  $result = reset($response->xpath('items/item'));

  drush_print('Archive create started, job=' . (string) $result->job_id . '. asset id = ' . (string) $result->asset_id);
}
