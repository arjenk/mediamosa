<?php
/**
 * @file
 * Rest call for archive.
 */

/**
 * URI: /archive/create
 * Method: GET
 *
 * Create a zip file for multiple assets.
 */
class mediamosa_rest_call_archive_create extends mediamosa_rest_call {
  // ------------------------------------------------------------------ Consts.
  // Rest vars;
  const ASSET_ID = 'asset_id';
  const PROFILE = 'profile';
  const FILENAME = 'filename';
  const ORIGINAL = 'original';
  const USER_ID = 'user_id';
  const GROUP_ID = 'group_id';
  const ACL_DOMAIN = 'acl_domain';
  const ACL_REALM = 'acl_realm';
  const IS_APP_ADMIN = 'is_app_admin';
  const VAR_IS_REQUIRED_NO = 'NO';
  const COMPLETED_TRANSCODING_URL = 'completed_transcoding_url';
  // @@todo type: zip/tgz

  // ------------------------------------------------------------ Get Var Setup.
  /**
   * Implements get_var_setup().
   */
  public function get_var_setup() {
    $var_setup = array();

    $var_setup = array(
      self::VARS => array(
        self::ASSET_ID => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_ASSET_ID,
          self::VAR_DESCRIPTION => 'The IDs of the assets to retrieve.',
          self::VAR_IS_REQUIRED => self::VAR_IS_REQUIRED_YES,
          self::VAR_IS_ARRAY => self::VAR_IS_ARRAY_YES,
        ),
        self::USER_ID => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_USER_ID,
          self::VAR_IS_REQUIRED => self::VAR_IS_REQUIRED_YES,
          self::VAR_DESCRIPTION => 'User id that performs request.',
        ),
        self::GROUP_ID => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_GROUP_ID,
          self::VAR_DESCRIPTION => 'The user group ID.',
          self::VAR_RANGE_END => mediamosa_user_group_db::GROUP_ID_LENGTH,
        ),
        self::ACL_DOMAIN => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_DOMAIN,
          self::VAR_DESCRIPTION => 'Authentication domain parameter.',
        ),
        self::ACL_REALM => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_REALM,
          self::VAR_DESCRIPTION => 'Authentication realm parameter.',
        ),
        self::IS_APP_ADMIN => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_BOOL,
          self::VAR_DESCRIPTION => 'Admin has right to all assets.',
          self::VAR_DEFAULT_VALUE => 'FALSE',
        ),
        self::PROFILE => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_STRING,
          self::VAR_DESCRIPTION => 'Selection of mediafiles, select specific transcode profile(s), default original.',
          self::VAR_IS_REQUIRED => self::VAR_IS_REQUIRED_NO,
          self::VAR_DEFAULT_VALUE => NULL,
        ),
        self::FILENAME => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_STRING,
          self::VAR_DESCRIPTION => 'Name of archive filename.',
          self::VAR_IS_REQUIRED => self::VAR_IS_REQUIRED_NO,
          self::VAR_DEFAULT_VALUE => NULL,
        ),
        self::COMPLETED_TRANSCODING_URL => array(
          self::VAR_TYPE => mediamosa_sdk::TYPE_URL,
          self::VAR_DESCRIPTION => 'Triggered when transcoding is completed.',
        ),
      ),
    );

    // Include default.
    return self::get_var_setup_default($var_setup);
  }

  // ------------------------------------------------------------------ Do Call.
  /**
   * Implements do_call().
   */
  public function do_call() {
    $mediamosa = mediamosa::get();

    $app_ids = $this->get_param_value_app();
    $app_id = reset($app_ids);
    $is_app_admin = $this->get_param_value(self::IS_APP_ADMIN);

    if (!(isset($app_id))) {
      $app_id = 1;
    }
    // User, group and acls for authentication.
    $user_id = $this->get_param_value(self::USER_ID);
    $group_id = $this->get_param_value(self::GROUP_ID);
    $acl_domain = $this->get_param_value(self::ACL_DOMAIN);
    $acl_realm = $this->get_param_value(self::ACL_REALM);

    $completed_transcoding_url = $this->get_param_value(self::COMPLETED_TRANSCODING_URL);

    // Asset ids to download.
    $asset_ids = $this->get_param_value(self::ASSET_ID);

    foreach ($asset_ids as $asset_id) {
      // Will fix some notices when we supply array in array.
      if (is_array($asset_id)) {
        throw new mediamosa_exception_error(mediamosa_error::ERRORCODE_VALIDATE_FAILED, array('@param' => self::ASSET_ID, '@type' => mediamosa_sdk::TYPE_ASSET_ID));
      }
    }

    // What mediafiles to download.
    $download_profile_id = $this->get_param_value(self::PROFILE);

    // Name of file.
    $zip_filename = $this->get_param_value(self::FILENAME);
    if (!isset($zip_filename) || trim($zip_filename) == '') {
      $zip_filename = 'download';
    }

    // Create an asset+mediafile to store the download.
    $zip_asset_id = mediamosa_asset::create($app_id, $user_id, $group_id, NULL, NULL, NULL, 'download');

    // Insert a mediafile as this is required for transcode..
    $mediafile_id = mediamosa_db::uuid($app_id);
    mediamosa_asset_mediafile::create($mediafile_id, $app_id, $zip_asset_id, $user_id,
      array(mediamosa_asset_mediafile_db::IS_ORIGINAL_FILE => 'TRUE'));

    // Collect the mediafile_id's to archive.
    $mediafiles = array();
    $options = array();
    foreach ($asset_ids as $asset_id) {
      if (((int) $download_profile_id > 0) && ($download_profile_id != 'original')) {
        $options['transcode_profile_id'] = $download_profile_id;
      }
      else {
        $options['get_originals_only'] = TRUE;
      }

      $mediafile = mediamosa_asset_mediafile::get_by_asset_id($asset_id, array('mediafile_id', 'filename'), $options);
      $mediafile = reset($mediafile);

      if (!isset($mediafile['mediafile_id'])) {
        continue;
      }
      // Check access on the mediafile.
      try {
        mediamosa_acl::access_check_on_object(
          mediamosa_acl::ACL_TYPE_MEDIAFILE,
          $asset_id,
          $mediafile['mediafile_id'],
          $app_id,
          $user_id,
          $group_id,
          $acl_domain,
          $acl_realm,
          $is_app_admin
        );
        $mediafiles[$mediafile['mediafile_id']] = $mediafile['filename'];
      }
      catch (mediamosa_exception_error_access_denied $e) {
        // No access, just skip the mediafile.
      }
    }

    $priority = 0;
    $command = mediamosa_transcode_profile::arrayToCommand(
      array(
        'mediafiles' => serialize($mediafiles),
        'filename' => $zip_filename,
      )
    );
    $job_id = mediamosa_job::create_job_transcode($app_id, $user_id, $group_id, $is_app_admin, $mediafile_id, FALSE, array(), 'zip', $command, 'archive', NULL, $completed_transcoding_url, $priority);

    // Return the created job_id plus asset_id.
    $mediamosa->add_item(array('job_id' => $job_id, 'asset_id' => $zip_asset_id));
  }
}
