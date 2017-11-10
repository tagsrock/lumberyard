#
# All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
# its licensors.
#
# For complete copyright and license terms please see the LICENSE at the root of this
# distribution (the "License"). All use of this software is governed by the License,
# or, if provided, by the license below or the license accompanying this file. Do not
# remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#

import unittest
import mock

import service
import errors
import mock_handler

class UnitTest_CloudGemFramework_DefaultServiceApiContent_ServiceApiDispatch(unittest.TestCase):


    def setUp(self):
        mock_handler.reset()

    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_normal_alone(self, mock_import_module):
        
        event = {
            'module': 'test_module',
            'function': 'test_function_alone',
            'parameters': {
                'param_a': 'param_a_value'
            }
        }
        
        context = {}
        
        mock_handler.response = 'expected response'
        
        response = service.dispatch(event, context)

        self.assertEquals(mock_handler.response, response)
        self.assertEquals(event['parameters']['param_a'], mock_handler.received_param_a)
        self.assertIs(event, mock_handler.received_request.event)
        self.assertIs(context, mock_handler.received_request.context)

        mock_import_module.assert_called_once_with('api.test_module')


    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_normal_first(self, mock_import_module):
        
        event = {
            'module': 'test_module',
            'function': 'test_function_first',
            'parameters': {
                'param_a': 'param_a_value',
                'param_b': 'param_b_value'
            }
        }
        
        context = {}
        
        mock_handler.response = 'expected response'
        
        response = service.dispatch(event, context)

        self.assertEquals(mock_handler.response, response)
        self.assertEquals(event['parameters']['param_a'], mock_handler.received_param_a)
        self.assertEquals(event['parameters']['param_b'], mock_handler.received_param_b)
        self.assertIs(event, mock_handler.received_request.event)
        self.assertIs(context, mock_handler.received_request.context)

        mock_import_module.assert_called_once_with('api.test_module')


    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_normal_last(self, mock_import_module):
        
        event = {
            'module': 'test_module',
            'function': 'test_function_last',
            'parameters': {
                'param_a': 'param_a_value',
                'param_b': 'param_b_value'
            }
        }
        
        context = {}
        
        mock_handler.response = 'expected response'
        
        response = service.dispatch(event, context)

        self.assertEquals(mock_handler.response, response)
        self.assertEquals(event['parameters']['param_a'], mock_handler.received_param_a)
        self.assertEquals(event['parameters']['param_b'], mock_handler.received_param_b)
        self.assertIs(event, mock_handler.received_request.event)
        self.assertIs(context, mock_handler.received_request.context)

        mock_import_module.assert_called_once_with('api.test_module')


    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_normal_middle(self, mock_import_module):
        
        event = {
            'module': 'test_module',
            'function': 'test_function_middle',
            'parameters': {
                'param_a': 'param_a_value',
                'param_b': 'param_b_value',
                'param_c': 'param_c_value'
            }
        }
        
        context = {}
        
        mock_handler.response = 'expected response'
        
        response = service.dispatch(event, context)

        self.assertEquals(mock_handler.response, response)
        self.assertEquals(event['parameters']['param_a'], mock_handler.received_param_a)
        self.assertEquals(event['parameters']['param_b'], mock_handler.received_param_b)
        self.assertEquals(event['parameters']['param_c'], mock_handler.received_param_c)
        self.assertIs(event, mock_handler.received_request.event)
        self.assertIs(context, mock_handler.received_request.context)

        mock_import_module.assert_called_once_with('api.test_module')


    def test_missing_event_module(self):

        event = {
            # missing: 'module': 'test_module',
            'function': 'test_function_without_decorator',
            'parameters': {
                'param_a': 'param_a_value'
            }
        }
        
        context = {}

        with self.assertRaisesRegexp(ValueError, 'No "module" property'):
            service.dispatch(event, context)


    def test_missing_event_function(self):

        event = {
            'module': 'test_module',
            # missing: 'function': 'test_function_without_decorator',
            'parameters': {
                'param_a': 'param_a_value'
            }
        }
        
        context = {}

        with self.assertRaisesRegexp(ValueError, 'No "function" property'):
            service.dispatch(event, context)


    @mock.patch('importlib.import_module', side_effect = ImportError)
    def test_no_such_module(self, mock_import_module):

        event = {
            'module': 'does_not_exist',
            'function': 'test_function',
            'parameters': {
                'param_a': 'param_a_value'
            }
        }
        
        context = {}

        with self.assertRaises(ImportError):
            service.dispatch(event, context)

        mock_import_module.assert_called_once_with('api.does_not_exist')


    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_no_such_function(self, mock_import_module):

        event = {
            'module': 'test_module',
            'function': 'does_not_exist',
            'parameters': {
                'param_a': 'param_a_value'
            }
        }
        
        context = {}

        with self.assertRaisesRegexp(ValueError, 'does not have an "does_not_exist" attribute'):
            service.dispatch(event, context)

        mock_import_module.assert_called_once_with('api.test_module')


    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_not_a_function(self, mock_import_module):

        event = {
            'module': 'test_module',
            'function': 'test_not_a_function',
            'parameters': {
                'param_a': 'param_a_value'
            }
        }
        
        context = {}

        with self.assertRaisesRegexp(ValueError, 'attribute "test_not_a_function" is not a function'):
            service.dispatch(event, context)

        mock_import_module.assert_called_once_with('api.test_module')


    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_missing_decorator(self, mock_import_module):

        event = {
            'module': 'test_module',
            'function': 'test_function_without_decorator',
            'parameters': {
                'param_a': 'param_a_value'
            }
        }
        
        context = {}

        with self.assertRaisesRegexp(ValueError, 'not a service dispatch handler'):
            service.dispatch(event, context)

        mock_import_module.assert_called_once_with('api.test_module')


    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_missing_parameters_when_no_parameters_expected(self, mock_import_module):

        event = {
            'module': 'test_module',
            'function': 'test_function_without_parameters'
            # mssing 'parameters': {
            #    'param_a': 'param_a_value'
            #}
        }
        
        context = {}

        mock_handler.response = 'expected response'
        
        response = service.dispatch(event, context)

        self.assertEquals(mock_handler.response, response)
        self.assertIs(event, mock_handler.received_request.event)
        self.assertIs(context, mock_handler.received_request.context)

        mock_import_module.assert_called_once_with('api.test_module')


    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_missing_parameters_when_parameters_expected(self, mock_import_module):

        event = {
            'module': 'test_module',
            'function': 'test_function_alone'
            # mssing 'parameters': {
            #    'param_a': 'param_a_value'
            #}
        }
        
        context = {}

        with self.assertRaisesRegexp(errors.ClientError, 'Expected the following parameters: param_a'):
            service.dispatch(event, context)

        mock_import_module.assert_called_once_with('api.test_module')


    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_missing_parameter_alone(self, mock_import_module):

        event = {
            'module': 'test_module',
            'function': 'test_function_alone',
            'parameters': {
                # missing:   'param_a': 'param_a_value'
            }
        }
        
        context = {}

        with self.assertRaisesRegexp(errors.ClientError, 'Expected the following parameters: param_a'):
            service.dispatch(event, context)

        mock_import_module.assert_called_once_with('api.test_module')


    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_missing_parameter_first(self, mock_import_module):

        event = {
            'module': 'test_module',
            'function': 'test_function_first',
            'parameters': {
                # missing:   'param_a': 'param_a_value'
                'param_b': 'param_b_value'
            }
        }
        
        context = {}

        with self.assertRaisesRegexp(errors.ClientError, 'Expected the following parameters: param_a'):
            service.dispatch(event, context)

        mock_import_module.assert_called_once_with('api.test_module')


    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_missing_parameter_last(self, mock_import_module):

        event = {
            'module': 'test_module',
            'function': 'test_function_last',
            'parameters': {
                # missing:   'param_a': 'param_a_value'
                'param_b': 'param_b_value'
            }
        }
        
        context = {}

        with self.assertRaisesRegexp(errors.ClientError, 'Expected the following parameters: param_a'):
            service.dispatch(event, context)

        mock_import_module.assert_called_once_with('api.test_module')


    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_missing_parameter_middle(self, mock_import_module):

        event = {
            'module': 'test_module',
            'function': 'test_function_middle',
            'parameters': {
                # missing:   'param_a': 'param_a_value'
                'param_b': 'param_b_value',
                'param_c': 'param_c_value',
            }
        }
        
        context = {}

        with self.assertRaisesRegexp(errors.ClientError, 'Expected the following parameters: param_a'):
            service.dispatch(event, context)

        mock_import_module.assert_called_once_with('api.test_module')


    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_extra_parameter(self, mock_import_module):

        event = {
            'module': 'test_module',
            'function': 'test_function_alone',
            'parameters': {
                'param_a': 'param_a_value',
                'param_b': 'param_b_value'
            }
        }
        
        context = {}

        with self.assertRaisesRegexp(errors.ClientError, 'The following parameters are unexpected: param_b'):
            service.dispatch(event, context)

        mock_import_module.assert_called_once_with('api.test_module')


    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_extra_parameter_with_kwargs(self, mock_import_module):

        event = {
            'module': 'test_module',
            'function': 'test_function_with_kwargs',
            'parameters': {
                'param_a': 'param_a_value',
                'param_b': 'param_b_value'
            }
        }
        
        context = {}

        mock_handler.response = 'expected response'
        
        response = service.dispatch(event, context)

        self.assertEquals(mock_handler.response, response)
        self.assertEquals({ 'param_b': event['parameters']['param_b'] }, mock_handler.received_kwargs)
        self.assertIs(event, mock_handler.received_request.event)
        self.assertIs(context, mock_handler.received_request.context)

        mock_import_module.assert_called_once_with('api.test_module')


    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_parameter_value_with_default_value(self, mock_import_module):

        event = {
            'module': 'test_module',
            'function': 'test_function_with_default_value',
            'parameters': {
                'param_a': 'param_a_value'
            }
        }
        
        context = {}

        mock_handler.response = 'expected response'
        
        response = service.dispatch(event, context)

        self.assertEquals(mock_handler.response, response)
        self.assertEquals(event['parameters']['param_a'], mock_handler.received_param_a)
        self.assertIs(event, mock_handler.received_request.event)
        self.assertIs(context, mock_handler.received_request.context)

        mock_import_module.assert_called_once_with('api.test_module')


    @mock.patch('importlib.import_module', return_value = mock_handler)
    def test_no_parameter_value_with_default_value(self, mock_import_module):

        event = {
            'module': 'test_module',
            'function': 'test_function_with_default_value',
            'parameters': {
                # missing: 'param_a': 'param_a_value'
            }
        }
        
        context = {}

        mock_handler.response = 'expected response'
        
        response = service.dispatch(event, context)

        self.assertEquals(mock_handler.response, response)
        self.assertEquals('default param a', mock_handler.received_param_a)
        self.assertIs(event, mock_handler.received_request.event)
        self.assertIs(context, mock_handler.received_request.context)

        mock_import_module.assert_called_once_with('api.test_module')

