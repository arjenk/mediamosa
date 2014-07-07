<?php
/**
 * @file
 * Archive tool functions.
 */

class mediamosa_tool_archive {
  /**
   * Name / ID of the module.
   */
  const NAME = 'archive';

  /**
   * Log for archive tool.
   *
   * @param string $message
   *   message to log.
   * @param array $variables
   *   variables to be used in the log message.
   * @param string $severity
   *   see WATCHDOG_* severity.
   */
  public static function log($message, array $variables = array(), $severity = WATCHDOG_NOTICE) {
    mediamosa_watchdog::log($message, $variables, $severity, 'tool archive');
  }

  /**
   * Returns TRUE when we support this mime-type.
   *
   * @param string $mime_type
   *   mimetype request.
   */
  public static function is_supported($mime_type) {
    // All files can be archived.
    return TRUE;
  }

  /**
   * Get the exec string for creating a archivefile.
   *
   * @param array $options
   *   Options array for archive.
   */
  public static function get_transcode_exec($options) {
    $params = explode(' ', $options['parameter_string']);

    $key = array_search('mediafiles', $params);
    $mediafiles = unserialize($params[$key + 1]);

    // $key = array_search('filename', $params);
    // $archivefilename = unserialize($params[$key + 1]);

    $files_to_archive = array();
    $tmp_dir = dirname($options['location_dest_file']) . DIRECTORY_SEPARATOR . $options['job_id'] . DIRECTORY_SEPARATOR;

    $prepare_archivefile = strtr('mkdir @file_location; cd @file_location;', array('@file_location' => $tmp_dir));
    foreach ($mediafiles as $mediafile_id => $filename) {
      $files_to_archive[] = mediamosa_storage::get_realpath_mediafile($mediafile_id);
      $ln_options = array(
        '@mediafile_id' => $mediafile_id,
        '@filename' => $filename,
        '@mfpath' => mediamosa_storage::get_realpath_mediafile($mediafile_id),
      );
      $prepare_archivefile .= strtr('mkdir @mediafile_id; ln -s @mfpath @mediafile_id/@filename;', $ln_options);
    }
    $prepare_archivefile .= 'cd ..';

    // Build the values.
    $execution_string_options = array(
      '@source_files' => $tmp_dir,
      '@mediafile_dest' => $options['location_dest_file'] . '.' . $options['file_extension'],
      '@status_file' => $options['status_file'],
      '@prepare_archivefile' => $prepare_archivefile,
    );

    // Execution string.
    // @@todo: make switch for tgz/zip.
    return strtr(
      '{ echo "Errors: none"; echo "Status: done"; echo "Progress: 0.000"; echo "Warnings: none"; } > @status_file;' .
      '@prepare_archivefile; ' .
      'cd @source_files;' .
      'nice zip -r @mediafile_dest  *;' .
      // 'nice tar chzvf @mediafile_dest *; ' .
      'cd ..; ' .
      '{ echo "Errors: none"; echo "Status: done"; echo "Progress: 1.000"; echo "Warnings: none"; } > @status_file;',
      $execution_string_options);
  }

  /**
   * Hook perform post transcode action.
   */
  public static function post_transcode($asset_id, $mediafile_id, $mediafile_id_dest, $file_ext) {

    // Update asset_type: set downloadable.
    // @todo: get filename from job parameters.
    mediamosa_db::db_update(mediamosa_asset_mediafile_db::TABLE_NAME)
      ->fields(array(
          mediamosa_asset_mediafile_db::IS_DOWNLOADABLE => mediamosa_asset_mediafile_db::IS_DOWNLOADABLE_TRUE,
          mediamosa_asset_mediafile_db::FILENAME => 'download.zip',
        ))
      ->condition(mediamosa_asset_mediafile_db::ID, $mediafile_id_dest)
      ->execute();
  }
}
